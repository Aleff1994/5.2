#include "stdafx.h"
#include "NewUISystem.h"
#include "ZzzInterface.h"
#include "CGMCameraWorld.h"
#include "WINHANDLE.h"
#include "TextClien.h"
extern char Mess0[256];
extern char Mess1[256];
extern void LoadCameraText();

CGMCameraWorld::CGMCameraWorld()
{
	m_Enable = 0;
	m_IsMove = 0;
	m_IsZoom = 0;
	cursor.X = 0;
	cursor.Y = 0;
	zoomPercent[0] = 40.0;
	zoomPercent[1] = 180.0;
	zoomPrecision = 2.0;

	m_RotationSpeedX = 0.0f;
	m_RotationSpeedY = 0.0f;

	m_Default = new CAMERA_INFO();
	m_Current = new CAMERA_INFO();
	m_nBackup = new CAMERA_INFO();

	m_Default->IsLoad = 0;

	// Preset amplo e mais alto, inspirado na camera do video de referencia.
	// F11 continua restaurando os valores classicos abaixo.
	m_Current->zoom = 42.0f;
	m_Current->Rot.Y = -62.0f;
	m_Current->Rot.Z = 393.0;
	m_Current->camAngle = 62.0f;

	m_Default->zoom = 35.0;
	m_Default->Rot.Y = -48.5;
	m_Default->Rot.Z = 150.0;
	m_Default->camAngle = 45.0;
}

CGMCameraWorld::~CGMCameraWorld()
{
	delete m_Default;
	delete m_Current;
	delete m_nBackup;
}

void CGMCameraWorld::Init()
{
	ResolutionConfig* conf = gwinhandle->LoadCurrentConfig();

	m_Current->camWidthFar = 2000.0 * g_fScreenRate_x;
	m_Default->camWidthFar = 2000.0 * g_fScreenRate_x;
	//--
	m_Current->WidthFar[0] = 3000.0 * g_fScreenRate_x;
	m_Current->WidthFar[1] = 2250.0 * g_fScreenRate_x;
	m_Current->WidthFar[2] = 1300.0 * g_fScreenRate_x;
	m_Current->WidthFar[3] = 1250.0 * g_fScreenRate_x;
	m_Current->WidthFar[5] = 1900.0 * g_fScreenRate_x;
	//--
	m_Current->WidthNear[0] = 540.0 * g_fScreenRate_y;
	m_Current->WidthNear[1] = 540.0 * g_fScreenRate_y;
	m_Current->WidthNear[2] = 580.0 * g_fScreenRate_y;
	m_Current->WidthNear[3] = 540.0 * g_fScreenRate_y;
	m_Current->WidthNear[5] = 600.0 * g_fScreenRate_y;
	//--
	m_Current->camviewFar[0] = 8500.0 * g_fScreenRate_x; //-- WD_6STADIUM
	m_Current->camviewFar[1] = 5100.0 * g_fScreenRate_x; //-- WD_30BATTLECASTLE_2
	m_Current->camviewFar[2] = 3300.0 * g_fScreenRate_x; //-- WD_30BATTLECASTLE
	m_Current->camviewFar[3] = 2850.0 * g_fScreenRate_x; //-- WD_62SANTA_TOWN
	m_Current->camviewFar[5] = 3300.0 * g_fScreenRate_x;

	if (conf)
	{
		m_Current->WidthFar[4] = conf->width_far;
		m_Current->WidthNear[4] = conf->width_near;
		m_Current->camviewFar[4] = conf->view_far; //-- General
	}
	else
	{
		m_Current->WidthFar[4] = 1190.0 * g_fScreenRate_x;
		m_Current->WidthNear[4] = 540.0 * g_fScreenRate_y;
		m_Current->camviewFar[4] = 2800.0 * g_fScreenRate_x; //-- General
	}

	memcpy(m_nBackup, m_Current, sizeof(CAMERA_INFO));
	memcpy(m_Default->WidthFar, m_Current->WidthFar, sizeof(m_Default->WidthFar));
	memcpy(m_Default->WidthNear, m_Current->WidthNear, sizeof(m_Default->WidthNear));
	memcpy(m_Default->camviewFar, m_Current->camviewFar, sizeof(m_Default->camviewFar));
}
//CAMERA3D ATIVANDO
void CGMCameraWorld::Toggle()
{
	// Força a leitura do arquivo .ini toda vez que você aperta F10
	LoadCameraText();

	m_Enable = !m_Enable;
	m_IsMove = FALSE;
	m_RotationSpeedX = m_RotationSpeedY = 0.f;
	if (m_Enable) m_Current->Rot.X = CameraAngle[2];

	// Se aparecer "ERRO_MESS0" na tela, o problema é o caminho do arquivo (path)
	if (m_Enable != 0)
		CreateNotice(Mess0, 2);
	else
		CreateNotice(Mess1, 3);

	if (((m_Default->IsLoad == 0) ? (m_Default->IsLoad++) : m_Default->IsLoad) == 0)
	{
		m_Default->Rot.X = CameraAngle[2];
	}
}

void CGMCameraWorld::Backup()
{
	if (this->m_Enable != 0 && SceneFlag == MAIN_SCENE)
	{
		this->DefaultValues();
	}
}

bool CGMCameraWorld::IsEnable()
{
	return this->m_Enable!=0;
}

void CGMCameraWorld::DefaultValues()
{
	m_IsMove = FALSE;
	m_RotationSpeedX = m_RotationSpeedY = 0.f;
	PreparedZoom();
	CameraAngle[2] = m_Default->Rot.X;
	m_Current->zoom = m_Default->zoom;
	m_Current->Rot.X = m_Default->Rot.X;
	m_Current->Rot.Y = m_Default->Rot.Y;
	m_Current->Rot.Z = m_Default->Rot.Z;
	m_Current->camAngle = m_Default->camAngle;
	m_Current->camWidthFar = m_Default->camWidthFar;
	memcpy(m_Current->WidthFar, m_Default->WidthFar, sizeof(m_Current->WidthFar));
	memcpy(m_Current->WidthNear, m_Default->WidthNear, sizeof(m_Current->WidthFar));
	memcpy(m_Current->camviewFar, m_Default->camviewFar, sizeof(m_Current->camviewFar));
}

void CGMCameraWorld::recover_backup_cam()
{
	memcpy(m_Current, m_nBackup, sizeof(CAMERA_INFO));
}

void CGMCameraWorld::CalcNearFar()
{
	float percent = (float)(m_Current->zoom / (m_Default->zoom / 100.f));
	// Keep the 3D camera movement/FOV intact, but do not let distant-world
	// visibility grow without bound as the user zooms out.  On legacy OpenGL
	// that expansion multiplies terrain and static-object work even though the
	// foreground appearance is unchanged.
	static const bool stableCameraFrustum = true;
	static const int configuredMaxRenderPercent = 120;
	static const float maxRenderPercent = static_cast<float>(
		configuredMaxRenderPercent < 100 ? 100 :
		(configuredMaxRenderPercent > 180 ? 180 : configuredMaxRenderPercent));
	const float renderPercent = stableCameraFrustum ? (std::min)(percent, maxRenderPercent) : percent;

	m_Current->camWidthFar = (float)CalcFactor(m_Default->camWidthFar, renderPercent) + (abs((m_Current->Rot.Z - m_Default->Rot.Z)) * 3.0f);

	for (int i = 0; i < 6; i++)
	{
		m_Current->camviewFar[i] = (float)CalcFactor(m_Default->camviewFar[i], renderPercent) + (abs((m_Current->Rot.Z - m_Default->Rot.Z)) * 2.0f);

		m_Current->WidthFar[i] = (double)CalcFactor(m_Default->WidthFar[i], renderPercent) + (abs((m_Current->Rot.Z - m_Default->Rot.Z)) * 2.0f);

		m_Current->WidthNear[i] = (double)CalcFactor(m_Default->WidthNear[i], renderPercent) + (abs((m_Current->Rot.Z - m_Default->Rot.Z)) * 2.0f);
	}
	memcpy(m_nBackup, m_Current, sizeof(CAMERA_INFO));
}

void CGMCameraWorld::PreparedZoom()
{
	zoomfade = 0;

	m_IsZoom = 0;

	zoomFactor = 0.0;

	desiredZoomFactor = 0;

	current_scroll_value = 0;

	smoothed_zoom_factor = 0;

	zoomLimit[0] = (m_Default->zoom / 100.f) * zoomPercent[0];

	zoomLimit[1] = (m_Default->zoom / 100.f) * zoomPercent[1];
}

void CGMCameraWorld::ZoomInNearFar()
{
	if (!g_pNewUISystem->CheckMouseUse() && MouseOnWindow == 0
		&& SEASON3B::CheckMouseIn(0, 0, GetScreenWidth(), GetWindowsY)
		&& g_dwMouseUseUIID == 0)
	{
		if (m_Enable == 0 || SceneFlag != MAIN_SCENE)
		{
			m_IsZoom = 0;
			return;
		}

		if (MouseWheel < 0 || MouseWheel > 0)
		{
			PreparedZoom();
			m_IsZoom = TRUE;
			zoomfade = (MouseWheel > 0) ? 1 : 2;
			zoomFactor = m_Current->zoom;
			MouseWheel = 0;
		}

		if (zoomfade == 1)
			desiredZoomFactor = (zoomFactor - zoomPrecision);

		if (zoomfade == 2)
			desiredZoomFactor = (zoomFactor + zoomPrecision);

		zoomfade = 0;

		if (m_IsZoom)
		{
			if (smoothed_zoom_factor == desiredZoomFactor)
			{
				PreparedZoom();
				return;
			}
			float mu = current_scroll_value++ / 60.f;

			smoothed_zoom_factor = hermite_interpolation(zoomFactor, desiredZoomFactor, 0.0, 0.0, mu);

			if (smoothed_zoom_factor >= zoomLimit[0] && smoothed_zoom_factor <= zoomLimit[1])
			{
				m_Current->zoom = smoothed_zoom_factor;

				float percent = (float)(m_Current->zoom / (m_Default->zoom / 100.f));

				if (percent < 75.0)
				{
					if (zoomfade == 1)
					{
						if (m_Current->Rot.Y > -90.0f)
						{
							SetAngleY(-2.42, -44.0);
						}
					}
					else if (zoomfade == 2)
					{
						if (m_Current->Rot.Y < -45.0f)
						{
							SetAngleY(2.42, 44.0);
						}
					}
				}

				m_Current->zoom = smoothed_zoom_factor;
				CalcNearFar();
			}
			else
			{
				PreparedZoom();
			}
		}
	}
}

void CGMCameraWorld::RotateInNearFar()
{
    if (!m_Enable || SceneFlag != MAIN_SCENE || !gwinhandle->CheckWndActive()
        || g_pNewUISystem->CheckMouseUse() || MouseOnWindow || g_dwMouseUseUIID)
    {
        m_IsMove = FALSE;
        return;
    }
    const bool held = SEASON3B::IsPress(VK_MBUTTON) || SEASON3B::IsRepeat(VK_MBUTTON);
    if (!held) { m_IsMove = FALSE; return; }
    if (!m_IsMove)
    {
        cursor.X = MouseX;
        cursor.Y = MouseY;
        m_IsMove = TRUE;
        return;
    }
    // Same direct yaw response as the mobile gesture (240 degrees per screen).
    const float dx = (MouseX - cursor.X) * (240.f / GetScreenWidth());
    const float dy = (MouseY - cursor.Y) * 0.10f;
    cursor.X = MouseX;
    cursor.Y = MouseY;
    if (dx != 0.f) SetAngleX(dx);
    if (dy != 0.f) SetAngleY(dy, dy * 18.f);
}
void CGMCameraWorld::SetAngleX(float value)
{
    // Wrap without snapping back when crossing the end of a full rotation.
    m_Current->Rot.X = fmodf(m_Current->Rot.X + value + 720.f, 360.f);
    CameraAngle[2] = m_Current->Rot.X;
    m_Current->camAngle = -m_Current->Rot.X;
    CalcNearFar();
}
void CGMCameraWorld::SetAngleY(float value, float heightDelta)
{
    const float previous = m_Current->Rot.Y;
    m_Current->Rot.Y = (std::max)(-90.f, (std::min)(-45.f, previous + value));
    if (value != 0.f) m_Current->Rot.Z -= heightDelta * ((m_Current->Rot.Y - previous) / value);
    CalcNearFar();
}

float CGMCameraWorld::hermite_interpolation(float y0, float y1, float y2, float y3, float mu)
{
	float m0, m1, mu2, mu3;
	float a0, a1, a2, a3;

	mu = sin(mu * Q_PI / 2.0f); // Aplicar EaseOutSine

	mu2 = mu * mu;
	mu3 = mu2 * mu;
	m0 = (y1 - y0) * 0.5f + (y2 - y1) * 0.5f;
	m1 = (y2 - y1) * 0.5f + (y3 - y2) * 0.5f;
	a0 = 2 * mu3 - 3 * mu2 + 1;
	a1 = mu3 - 2 * mu2 + mu;
	a2 = mu3 - mu2;
	a3 = -2 * mu3 + 3 * mu2;

	return (a0 * y1 + a1 * m0 + a2 * m1 + a3 * y2);
}

CAMERA_INFO* CGMCameraWorld::CurrentCam()
{
	return m_Current;
}

float CGMCameraWorld::GetSmoothed_zoom_factor()
{
	return m_Current->zoom / m_Default->zoom;
}
