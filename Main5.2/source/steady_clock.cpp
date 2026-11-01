#include "stdafx.h"
#include "SkillManager.h"
#include "ZzzOpenglUtil.h"
#include "steady_clock.h"
#include "wsclientinline.h"
#include "Time/Timer.h"

namespace
{
	HANDLE GetHighResolutionFrameTimer()
	{
		typedef HANDLE(WINAPI* CreateWaitableTimerExWProc)(
			LPSECURITY_ATTRIBUTES, LPCWSTR, DWORD, DWORD);

		static HANDLE timer = []() -> HANDLE
		{
			HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
			if (kernel32 == NULL)
				return NULL;

			CreateWaitableTimerExWProc createTimer =
				reinterpret_cast<CreateWaitableTimerExWProc>(
					GetProcAddress(kernel32, "CreateWaitableTimerExW"));
			if (createTimer == NULL)
				return NULL;

			// CREATE_WAITABLE_TIMER_HIGH_RESOLUTION is available on Windows 10
			// version 1803 and later.  Resolve it dynamically so older systems
			// keep using the existing sleep-plus-short-spin fallback.
			return createTimer(NULL, NULL, 0x00000002,
				TIMER_MODIFY_STATE | SYNCHRONIZE);
		}();

		return timer;
	}

	bool WaitForFrameTimer(const std::chrono::steady_clock::time_point deadline)
	{
		HANDLE timer = GetHighResolutionFrameTimer();
		if (timer == NULL)
			return false;

		const std::chrono::steady_clock::time_point now =
			std::chrono::steady_clock::now();
		if (now >= deadline)
			return true;

		const LONGLONG nanoseconds =
			std::chrono::duration_cast<std::chrono::nanoseconds>(deadline - now).count();
		LARGE_INTEGER dueTime;
		dueTime.QuadPart = -((nanoseconds / 100) > 0 ? (nanoseconds / 100) : 1);

		if (!SetWaitableTimer(timer, &dueTime, 0, NULL, NULL, FALSE))
			return false;

		return WaitForSingleObject(timer, 50) == WAIT_OBJECT_0;
	}

	void RecordRuntimeFps(int sampledFps, int targetFps)
	{
		static int telemetryEnabled = 0;
		static int sampleCount = 0;
		static int sampleSum = 0;
		static int sampleMin = 10000;
		static int sampleMax = 0;

		if (telemetryEnabled == 0 || sampledFps <= 0)
		{
			return;
		}

		++sampleCount;
		sampleSum += sampledFps;
		sampleMin = (std::min)(sampleMin, sampledFps);
		sampleMax = (std::max)(sampleMax, sampledFps);

		// One append every five seconds is enough for diagnosis and avoids
		// introducing measurable I/O into the render loop.
		if (sampleCount < 5)
		{
			return;
		}

		char line[192];
		const int length = sprintf_s(line, sizeof(line),
			"Time=%lu Scene=%d FPS=%d Avg=%d Min=%d Max=%d Target=%d\r\n",
			GetTickCount(), SceneFlag, sampledFps, sampleSum / sampleCount,
			sampleMin, sampleMax, targetFps);

		HANDLE file = CreateFileA("RendererRuntime.log", FILE_APPEND_DATA,
			FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, NULL);
		if (file != INVALID_HANDLE_VALUE)
		{
			DWORD written = 0;
			WriteFile(file, line, length, &written, NULL);
			CloseHandle(file);
		}

		sampleCount = 0;
		sampleSum = 0;
		sampleMin = 10000;
		sampleMax = 0;
	}
}

csteady_clock::csteady_clock()
{
	ping_reg_id = 0;
	counterframe = 0;
	deltaAccumulated = 0.0;
	fpsNormalizer = REFERENCE_FPS;

	speedNormalizer = 1.0;
	normal_check = true;

	frame_limit = 1.f;
	mainthread = std::chrono::high_resolution_clock::now();
	last_check_time = std::chrono::high_resolution_clock::now();

	threadTime = new CTimer();

}

csteady_clock::~csteady_clock()
{
	SAFE_DELETE(threadTime);
}

bool csteady_clock::CheckNormalizer()
{
	return normal_check;
}

double csteady_clock::GetNormalizerFps()
{
	return speedNormalizer;
}

double csteady_clock::GetDeltAccumulated()
{
	return deltaAccumulated;
}

int csteady_clock::GetLimitFps()
{
	return gmProtect->ajust_fps_render;
}

int csteady_clock::Getframe_per_second()
{
	return 1000 / this->GetLimitFps();
}

void csteady_clock::normalizefps()
{
	if (this->GetLimitFps() == (int)REFERENCE_FPS)
	{
		normal_check = true;
		return;
	}

	auto current_time = std::chrono::high_resolution_clock::now();

	double elapsed_time = std::chrono::duration<double>(current_time - last_check_time).count();

	if (elapsed_time >= (0.04))
	{
		normal_check = true;
		last_check_time = current_time;
	}
	else
	{
		normal_check = false;
	}
}

void csteady_clock::LoadInformationFps()
{
	static bool timeinit = false;

	if (!timeinit)
	{
		timeinit = true;
		counterframe = 0;
		frame_limit = 1.0;
		FPS = this->GetLimitFps();
		mainthread = std::chrono::high_resolution_clock::now();
		save_time = threadTime->GetTimeElapsed();
	}

	WorldTime = static_cast<float>(timeGetTime());

	auto current_time = threadTime->GetTimeElapsed();
	double difTime = (current_time - save_time);

	DeltaT = (difTime <= 0.0) ? 0.01 : (difTime * 0.001); // it just cant be 0

	fpsNormalizer = std::ceil((1.0 / DeltaT));

	speedNormalizer = (double)min((REFERENCE_FPS / fpsNormalizer), 1.0);
	save_time = current_time;
	counterframe++;

	bool finishwating = frame_limit.hasElapsed();
	//
	if (finishwating)
	{
		FPS = counterframe > this->GetLimitFps() ? this->GetLimitFps() : counterframe;
		RecordRuntimeFps((int)FPS, this->GetLimitFps());
		counterframe = 0;
	}

	if (SceneFlag == MAIN_SCENE)
	{
		if (finishwating)
		{
			runtime_send_ping();
		}
		gSkillManager.CalcSkillDelay(static_cast<int>(difTime));
	}

	deltaAccumulated += speedNormalizer;

	normalizefps();
}

std::chrono::steady_clock::time_point csteady_clock::GetthreadTime()
{
	// Capture the start of the frame being rendered. The old implementation
	// returned the previous frame start, so thread_sleep alternated between a
	// full wait and almost no wait instead of pacing every frame uniformly.
	const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

	// Track how long the "outside" portion of the previous frame took (message
	// pump, network I/O, everything that runs before we get back here). If it's
	// small and sane, fold it into a smoothed average so thread_sleep can shave
	// that much off the next sleep instead of oversleeping to compensate.
	if (frameTimingInitialized)
	{
		const double targetFrameTime = 1.0 / static_cast<double>(GetLimitFps());
		const double measuredOutsideTime = std::chrono::duration<double>(now - lastFrameEnd).count();

		if (measuredOutsideTime >= 0.0 && measuredOutsideTime < (targetFrameTime * 0.5))
		{
			outsideFrameTime = (outsideFrameTime * 0.75) + (measuredOutsideTime * 0.25);
		}
		else
		{
			outsideFrameTime = 0.0;
		}
	}

	mainthread = now;
	return mainthread;
}

uintmax_t csteady_clock::thread_sleep(const std::chrono::steady_clock::time_point thread_tick)
{
	const double frameavg = max(0.0, (1.0 / (double)this->GetLimitFps()) - outsideFrameTime);
	auto current_time = std::chrono::steady_clock::now();
	double frame_time = std::chrono::duration<double>(current_time - thread_tick).count();

	if (frame_time < frameavg)
	{
		const auto frame_duration = std::chrono::duration<double>(frameavg);
		const auto deadline = thread_tick +
			std::chrono::duration_cast<std::chrono::steady_clock::duration>(frame_duration);

		// Avoid burning a CPU core for the final 2 ms of every frame.  A Windows
		// high-resolution waitable timer sleeps until 0.20 ms before the target;
		// only that very small remainder uses a precision spin.  Older Windows
		// versions retain the proven 2 ms sleep-plus-spin fallback.
		const auto precisionWindow = std::chrono::microseconds(200);
		const auto preciseDeadline = deadline - precisionWindow;
		if (current_time < preciseDeadline && !WaitForFrameTimer(preciseDeadline))
		{
			const auto fallbackWindow = std::chrono::milliseconds(2);
			const auto fallbackDeadline = deadline - fallbackWindow;
			if (current_time < fallbackDeadline)
				std::this_thread::sleep_until(fallbackDeadline);
		}
		while ((current_time = std::chrono::steady_clock::now()) < deadline)
		{
			YieldProcessor();
		}
		current_time = std::chrono::steady_clock::now();
		frame_time = std::chrono::duration<double>(current_time - thread_tick).count();
	}

	mainthread = current_time;
	lastFrameEnd = current_time;
	frameTimingInitialized = true;

	return static_cast<uintmax_t>(frame_time * 1000);
}

bool csteady_clock::rand_calc_check(int fr)
{
	static std::random_device rd;  // a seed source for the random number engine
	static std::mt19937 gen(rd()); // mersenne_twister_engine seeded with rd()
	static std::uniform_real_distribution<> distrib(0.0, 1.0);

	const auto rand_value = distrib(gen);
	const auto chance = (fr == 1) ? speedNormalizer : (1.0 / fr) * speedNormalizer;

	return rand_value <= chance;
}

void csteady_clock::runtime_send_ping()
{
	int ping_id = ping_reg_id++;

	SendPing(ping_id);

	pingMap[ping_id] = std::chrono::high_resolution_clock::now();
}

void csteady_clock::runtime_recv_ping(int ping_id)
{
	if (pingMap.find(ping_id) != pingMap.end())
	{
		auto endTime = std::chrono::high_resolution_clock::now();
		auto startTime = pingMap[ping_id];
		pingMap.erase(ping_id);
		ping_time = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
	}
}

int csteady_clock::get_ping_time()
{
	return ping_time;
}

csteady_clock* csteady_clock::Instance()
{
	static csteady_clock sInstance;
	return &sInstance;
}

DWORD standlimit(DWORD x)
{
	if (gsteady_clock->GetLimitFps() == (int)REFERENCE_FPS)
		return x;

	return (gsteady_clock->GetLimitFps() / REFERENCE_FPS * x);
}

double timepow(double x)
{
	if (gsteady_clock->GetLimitFps() == (int)REFERENCE_FPS)
		return x;

	//return pow(x, gsteady_clock->GetNormalizerFps());
	return pow(x, DeltaT * REFERENCE_FPS);
}

double timefac(double x)
{
	if (gsteady_clock->GetLimitFps() == (int)REFERENCE_FPS)
		return x;

	//return (x * gsteady_clock->GetNormalizerFps());
	return (x * DeltaT * REFERENCE_FPS);
}

double timeNormalizer(double x)
{
	if (gsteady_clock->GetLimitFps() == (int)REFERENCE_FPS)
		return x;

	return (x * gsteady_clock->GetNormalizerFps());
}

bool steady_clock_::numeral(int element) const
{
	if (_runvalueback >= _runvalue)
		return (_runvalueback >= element && _runvalue <= element);
	else
		return (_runvalue >= element && _runvalueback <= element);
}

bool steady_clock_::duration(int element)
{
	if (_runvalueback >= _runvalue)
	{
		return _runvalue <= (std::floor((_runvalueback / element)) * element);
	}
	else
	{
		return _runvalue >= (std::ceil((_runvalueback / element)) * element);
	}
}

bool steady_clock_::residual_duration(int element, int time)
{
	int multiplo;
	double residual_memory1, residual_memory2;

	if (_runvalueback >= _runvalue)
	{
		multiplo = (std::floor((_runvalueback / element)) * element);

		residual_memory1 = (_runvalue - multiplo);
		residual_memory2 = (_runvalueback - multiplo);

		return (residual_memory2 >= time && residual_memory1 <= time);
	}
	else
	{
		multiplo = (std::floor((_runvalue / element)) * element);

		residual_memory1 = (_runvalue - multiplo);
		residual_memory2 = (_runvalueback - multiplo);

		return (residual_memory2 <= time && residual_memory1 >= time);
	}
}

int steady_clock_::factor_res(int time)
{
	int rounded_back = 0;
	int rounded_value = 0;

	if (_runvalueback >= _runvalue)
	{
		rounded_back = static_cast<int>(std::floor(_runvalueback));
		rounded_value = static_cast<int>(std::ceil(_runvalue));
	}
	else
	{
		rounded_back = static_cast<int>(std::ceil(_runvalueback));
		rounded_value = static_cast<int>(std::floor(_runvalue));
	}

	if (rounded_back == rounded_value)
	{
		return (rounded_value % time);
	}
	return -1;
}
