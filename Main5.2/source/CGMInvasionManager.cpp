#include "stdafx.h"
#include "jpexs.h"
#include "UIBaseDef.h"
#include "UIControls.h"
#include "CGMInvasionManager.h"
#include "NewUISystem.h"
#include "ZzzCharacter.h"
#include "ZzzAI.h"
#include "MultiLanguage.h"

extern int FontHeight;
extern char* getMonsterName(int type);
extern bool CharacterAnimation(CHARACTER* c, OBJECT* o);

namespace
{
	const char* GetInvasionText(const char* english, const char* portuguese, const char* spanish)
	{
		if (g_pMultiLanguage == NULL)
			return english;

		switch (g_pMultiLanguage->GetLanguage())
		{
		case 1: return portuguese;
		case 2: return spanish;
		default: return english;
		}
	}

	const char* GetInvasionMonsterName(int monsterIndex)
	{
		const char* name = getMonsterName(monsterIndex);
		if (name != NULL && name[0] != '\0')
			return name;

		// O ID 42 (Red Dragon) nao existe nas tabelas NpcName originais desta base.
		if (monsterIndex == 42)
			return "Red Dragon";

		return GetInvasionText("Unknown monster", "Monstro desconhecido", "Monstruo desconocido");
	}
}

SEASON3B::CGMInvasionManager::CGMInvasionManager()
{
	m_CountActive = 0;
	is_Opentable = false;
	currentInvasion = -1;
	last_time = std::chrono::steady_clock::now();

	m_Pos.x = 0;
	m_Pos.y = 0;
	m_RenderFrameX = 0;
	m_RenderFrameY = 0;
	m_BossPreview = NULL;
	m_BossPreviewIndex = -1;
	m_BossPreviewRenderScale = 0.35f;
	m_BossPreviewCenterY = 0.0f;
	m_BossPreviewCenterZ = 0.0f;
	m_BossPreviewMeasuredSize = 0.0f;

	InvasionInfo.clear();
}

SEASON3B::CGMInvasionManager::~CGMInvasionManager()
{
	for (type_map_invasion::iterator it = InvasionInfo.begin(); it != InvasionInfo.end(); ++it)
	{
		it->second.total_monster.clear();
	}

	InvasionInfo.clear();
}

void SEASON3B::CGMInvasionManager::SetInvasion(int Index, DWORD TimeRemaing, char* Name)
{
	if (Index != -1 && Index >= 0 && Index < MAX_INVASION)
	{
		currentInvasion = Index;
		INVASION_GLOBAL_INFO& invasion = InvasionInfo[Index];

		// Asignar la nueva información
		invasion.index = Index;
		invasion.Name = Name; // Si 'Name' es un char*, usa std::string para asignar directamente
		invasion.TimeRemaing = TimeRemaing;
		// Limpiar los monstruos previos si los hubiera
		invasion.total_monster.clear();
	}
}

void SEASON3B::CGMInvasionManager::SetMonsterKill(int Index, int MonsterIndex, int Monster_Kill, int MonsterCount)
{
	if (Index != -1 && Index >= 0 && Index < MAX_INVASION)
	{
		INVASION_GLOBAL_INFO* invasion = FindInvasion(Index);

		if (invasion != NULL)
		{
			size_t length = invasion->total_monster.size();

			for (size_t i = 0; i < length; i++)
			{
				INVASION_MONSTER_INFO* info = &invasion->total_monster[i];

				if (info->MonsterIndex == MonsterIndex)
				{
					info->Monster_Kill = Monster_Kill;

					info->MonsterCount = MonsterCount;
				}
			}
		}
	}
}

void SEASON3B::CGMInvasionManager::SetMonsterCount(int Index, int MonsterIndex, int Monster_Kill, int MonsterCount)
{
	if (Index != -1 && Index >= 0 && Index < MAX_INVASION)
	{
		INVASION_GLOBAL_INFO* invasion = FindInvasion(Index);

		if (invasion != NULL)
		{
			invasion->total_monster.push_back({ MonsterIndex, Monster_Kill, MonsterCount });
		}
	}
}

void SEASON3B::CGMInvasionManager::Update()
{
	auto current_time = std::chrono::steady_clock::now();
	double difTime = std::chrono::duration<double>(current_time - last_time).count();

	if (difTime >= 1.0)
	{
		m_CountActive = 0;
		last_time = current_time;

		for (type_map_invasion::iterator it = InvasionInfo.begin(); it != InvasionInfo.end(); ++it)
		{
			INVASION_GLOBAL_INFO* invasion = &it->second;

			if (invasion->TimeRemaing > 0)
			{
				m_CountActive++;
				invasion->TimeRemaing -= 1;

				if (currentInvasion == -1)
				{
					currentInvasion = it->first;
				}
			}
			else
			{
				if (currentInvasion == it->first)
				{
					currentInvasion = -1;
				}
			}
		}
	}
}

void SEASON3B::CGMInvasionManager::SetPos(float RenderFrameX, float RenderFrameY)
{
	float RenderSizeX = 160.0;

	m_RenderFrameX = RenderFrameX;

	m_RenderFrameY = RenderFrameY;

	m_RenderFrameX -= (RenderSizeX / 2.f);
}

bool SEASON3B::CGMInvasionManager::UpdateMouseEvent()
{
	float RenderSizeX = 160.0;
	float RenderSizeY = 160.0;

	RenderSizeY = ((FontHeight + 2) * 2) / g_fScreenRate_y;

	if ((m_RenderFrameX + RenderSizeX) > GetScreenWidth()
		|| g_pNewUISystem->IsVisible(INTERFACE_MINI_MAP)
		|| g_pNewUISystem->IsVisible(INTERFACE_INGAMESHOP)
		|| g_pNewUISystem->IsVisible(INTERFACE_MASTER_LEVEL)
		)
	{
		return false;
	}

	if (SEASON3B::CheckMouseIn(m_RenderFrameX, m_RenderFrameY, RenderSizeX, RenderSizeY))
	{
		if (SEASON3B::IsRelease(VK_LBUTTON))
		{
			if (m_CountActive > 0 && SEASON3B::CheckMouseIn(m_RenderFrameX + 3.0, m_RenderFrameY + (RenderSizeY - 18.f) / 2.f, 17.f, 18.f))
			{
				this->PreviousInvasion();
			}
			else if (m_CountActive > 0 && SEASON3B::CheckMouseIn(m_RenderFrameX + RenderSizeX - 20.0, m_RenderFrameY + (RenderSizeY - 18.f) / 2.f, 17.f, 18.f))
			{
				this->NextInvasion();
			}
			else
			{
				is_Opentable = !is_Opentable;
			}
		}
		return true;
	}

	return false;
}

void SEASON3B::CGMInvasionManager::RenderFrame(float RenderFrameX, float RenderFrameY)
{
	float RenderSizeX = 160.0;
	float RenderSizeY = 160.0;
	float _ScreenX = 0.f, _ScreenY = 0.f;

	if ((m_RenderFrameX + RenderSizeX) > GetScreenWidth()
		|| g_pNewUISystem->IsVisible(INTERFACE_MINI_MAP)
		|| g_pNewUISystem->IsVisible(INTERFACE_INGAMESHOP)
		|| g_pNewUISystem->IsVisible(INTERFACE_MASTER_LEVEL)
		)
	{
		ClearBossPreview();
		return;
	}

	if (m_CountActive > 0)
	{
		INVASION_GLOBAL_INFO* Invasion = this->FindInvasion(currentInvasion);
		const INVASION_MONSTER_INFO* hoveredBoss = NULL;
		float hoveredBossY = 0.0f;

		_ScreenX = (m_RenderFrameX * g_fScreenRate_x);
		_ScreenY = (m_RenderFrameY * g_fScreenRate_y);

		RenderSizeY = ((FontHeight + 2) * 2);

		SEASON3B::RenderImageF(IMAGE_INVASION_SELECTOR, m_RenderFrameX, m_RenderFrameY, RenderSizeX, RenderSizeY / g_fScreenRate_y, 0.0, 0.0, 512.0, 96.0);

		g_pRenderText->SetFont(g_hFontBold);

		g_pRenderText->SetBgColor(0);
		g_pRenderText->SetTextColor(CLRDW_YELLOW);

		if (Invasion != NULL && Invasion->Name[0] != '\0')
		{
			g_pRenderText->RenderTextClipped(_ScreenX, _ScreenY, Invasion->Name.c_str(), (RenderSizeX * g_fScreenRate_x), RenderSizeY, RT3_SORT_CENTER);
		}
		else
		{
			g_pRenderText->RenderTextClipped(_ScreenX, _ScreenY,
				GetInvasionText("Invasion", "Invasao", "Invasion"),
				(RenderSizeX * g_fScreenRate_x), RenderSizeY, RT3_SORT_CENTER);
		}

		if (m_CountActive > 0)
		{
			RenderlookFetch(IMAGE_ND_BTN_L, m_RenderFrameX + 3.0, m_RenderFrameY + (RenderSizeY / g_fScreenRate_y - 18.f) / 2.f, true);

			RenderlookFetch(IMAGE_ND_BTN_R, m_RenderFrameX + RenderSizeX - 20.0, m_RenderFrameY + (RenderSizeY / g_fScreenRate_y - 18.f) / 2.f, true);
		}

		if (is_Opentable)
		{
			if (Invasion && Invasion->TimeRemaing > 0)
			{
				char pszText[100];
				size_t length = Invasion->total_monster.size();

				int totalseconds = Invasion->TimeRemaing;
				int hours = totalseconds / 3600;
				int minutes = (totalseconds / 60) % 60;
				int seconds = totalseconds % 60;

				sprintf_s(pszText, "%s: %02d:%02d:%02d",
					GetInvasionText("Time", "Tempo", "Tiempo"), hours, minutes, seconds);

				_ScreenY = RenderSizeY + 8 * g_fScreenRate_y;

				length += 2;
				RenderSizeY = ((FontHeight + 3) * length);
				length -= 2;

				SEASON3B::RenderImageF(IMAGE_INVASION_DETAILS, m_RenderFrameX, _ScreenY / g_fScreenRate_y, RenderSizeX, RenderSizeY / g_fScreenRate_y, 0.0, 0.0, 512.0, 320.0);

				RenderSizeY = (FontHeight * 2);

				g_pRenderText->SetTextColor(CLRDW_GOLD);
				g_pRenderText->RenderTextClipped(_ScreenX, _ScreenY, pszText, (RenderSizeX * g_fScreenRate_x), RenderSizeY, RT3_SORT_CENTER);

				_ScreenY += RenderSizeY;

				g_pRenderText->SetFont(g_hFont);

				g_pRenderText->SetTextColor(CLRDW_WHITE3);

				RenderSizeY = (FontHeight + 2);

				_ScreenX += 6 * g_fScreenRate_x;

				for (size_t i = 0; i < length; i++)
				{
					INVASION_MONSTER_INFO* Data = &Invasion->total_monster[i];
					const float rowX = _ScreenX / g_fScreenRate_x;
					const float rowY = _ScreenY / g_fScreenRate_y;
					const float rowHeight = RenderSizeY / g_fScreenRate_y;

					if (SEASON3B::CheckMouseIn(rowX, rowY, RenderSizeX - 12.0f, rowHeight))
					{
						hoveredBoss = Data;
						hoveredBossY = rowY;
					}

					sprintf_s(pszText, "- %s (%d / %d)", GetInvasionMonsterName(Data->MonsterIndex), Data->Monster_Kill, Data->MonsterCount);

					g_pRenderText->RenderTextClipped(_ScreenX, _ScreenY, pszText, 0, RenderSizeY, RT3_SORT_LEFT);
					_ScreenY += RenderSizeY;
				}
			}
		}

		if (hoveredBoss != NULL)
			RenderBossHover(hoveredBoss, hoveredBossY);
		else
			ClearBossPreview();
	}
	else
	{
		ClearBossPreview();
	}
}

void SEASON3B::CGMInvasionManager::RenderBossHover(const INVASION_MONSTER_INFO* selectedBoss, float anchorY)
{
	if (selectedBoss == NULL)
	{
		ClearBossPreview();
		return;
	}

	if (!EnsureBossPreview(selectedBoss->MonsterIndex))
		return;

	const float panelWidth = 154.0f;
	const float panelHeight = 170.0f;
	float panelX = m_RenderFrameX + 166.0f;
	if (panelX + panelWidth > GetWindowsX)
		panelX = m_RenderFrameX - panelWidth - 6.0f;
	panelX = max(2.0f, panelX);
	float panelY = max(2.0f, anchorY - 22.0f);
	if (panelY + panelHeight > GetWindowsY)
		panelY = max(2.0f, GetWindowsY - panelHeight - 2.0f);

	const float screenX = panelX * g_fScreenRate_x;
	const float screenY = panelY * g_fScreenRate_y;

	jpexs::RenderBoxjpexs_dds(IMAGE_HUD_PLAYER, screenX, screenY,
		panelWidth * g_fScreenRate_x, panelHeight);

	g_pRenderText->SetBgColor(0);
	g_pRenderText->SetFont(g_hFontBold);
	g_pRenderText->SetTextColor(CLRDW_GOLD);
	g_pRenderText->RenderTextClipped(screenX, screenY + 2.0f,
		GetInvasionMonsterName(selectedBoss->MonsterIndex), panelWidth * g_fScreenRate_x,
		FontHeight + 2, RT3_SORT_CENTER);

	RenderBossPreview3D(panelX + 5.0f, panelY + 19.0f,
		panelWidth - 10.0f, panelHeight - 48.0f);

	char bossText[128];
	sprintf_s(bossText, "%s: %d / %d",
		GetInvasionText("Defeated", "Eliminados", "Eliminados"),
		selectedBoss->Monster_Kill, selectedBoss->MonsterCount);
	g_pRenderText->SetFont(g_hFontBold);
	g_pRenderText->SetTextColor(CLRDW_YELLOW);
	g_pRenderText->RenderTextClipped(screenX + 4.0f * g_fScreenRate_x,
		screenY + panelHeight - FontHeight - 5.0f, bossText,
		(panelWidth - 8.0f) * g_fScreenRate_x, FontHeight + 2, RT3_SORT_CENTER);
}

void SEASON3B::CGMInvasionManager::ClearBossPreview()
{
	if (m_BossPreview != NULL && m_BossPreview->Object.Live)
		DeleteCharacter(m_BossPreview, &m_BossPreview->Object);

	m_BossPreview = NULL;
	m_BossPreviewIndex = -1;
	m_BossPreviewRenderScale = 0.35f;
	m_BossPreviewCenterY = 0.0f;
	m_BossPreviewCenterZ = 0.0f;
	m_BossPreviewMeasuredSize = 0.0f;
}

bool SEASON3B::CGMInvasionManager::EnsureBossPreview(int monsterIndex)
{
	if (m_BossPreview != NULL && m_BossPreview->Object.Live &&
		m_BossPreviewIndex == monsterIndex)
		return true;

	ClearBossPreview();
	m_BossPreview = CreateMonster(monsterIndex, 0, 0, -32000);
	if (m_BossPreview == NULL)
		return false;

	m_BossPreviewIndex = monsterIndex;
	m_BossPreviewRenderScale = min(0.78f, max(0.38f, m_BossPreview->Object.Scale * 0.55f));
	m_BossPreview->Key = -32000;
	m_BossPreview->Object.Live = true;
	m_BossPreview->Object.Visible = false;
	Vector(-300.0f, -300.0f, -300.0f, m_BossPreview->Object.Position);
	Vector(0.9f, 0.9f, 0.9f, m_BossPreview->Object.Light);
	SetAction(&m_BossPreview->Object, MONSTER01_STOP1);
	m_BossPreview->Object.AnimationFrame = 0.0f;
	m_BossPreview->Object.PriorAnimationFrame = 0.0f;
	return true;
}

void SEASON3B::CGMInvasionManager::RenderBossPreview3D(float x, float y, float width, float height)
{
	if (m_BossPreview == NULL || !m_BossPreview->Object.Live)
		return;

	CHARACTER* c = m_BossPreview;
	OBJECT* o = &c->Object;
	const float oldScale = o->Scale;
	vec3_t oldAngle;
	VectorCopy(o->Angle, oldAngle);
	const bool oldHideShadow = c->HideShadow;
	const bool oldFog = FogEnable;

	BMD* model = gmClientModels->GetModel(o->Type);
	if (model == NULL)
		return;

	// O preview usa o mesmo modelo carregado pelo mapa. Nao execute a IA completa
	// do monstro aqui: ela depende do mapa atual e pode alterar o boss real.
	// Avance apenas a animacao ociosa do modelo e restaure todo estado compartilhado.
	vec3_t oldBodyOrigin;
	vec3_t oldBodyAngle;
	vec3_t oldBodyLight;
	VectorCopy(model->BodyOrigin, oldBodyOrigin);
	VectorCopy(model->BodyAngle, oldBodyAngle);
	VectorCopy(model->BodyLight, oldBodyLight);
	const float oldBodyScale = model->BodyScale;
	const WORD oldModelAction = model->CurrentAction;
	model->PlayAnimation(&o->AnimationFrame, &o->PriorAnimationFrame,
		&o->PriorAction, 0.08f, o->Position, o->Angle);
	FogEnable = false;
	if (m_BossPreviewMeasuredSize > 1.0f)
	{
		const float targetSize = min(width - 8.0f, height - 14.0f);
		m_BossPreviewRenderScale = targetSize / m_BossPreviewMeasuredSize;
		m_BossPreviewRenderScale = min(0.95f, max(0.38f, m_BossPreviewRenderScale));
	}
	else
	{
		const float boxWidthY = o->BoundingBoxMax[1] - o->BoundingBoxMin[1];
		const float boxHeight = o->BoundingBoxMax[2] - o->BoundingBoxMin[2];
		const float horizontalSize = boxWidthY * (height / max(1.0f, width));
		const float fitSize = max(1.0f, max(boxHeight, horizontalSize));
		m_BossPreviewRenderScale = min(0.95f, max(0.38f, min(width - 8.0f, height - 14.0f) / fitSize));
	}

	const float modelCenterY = (m_BossPreviewMeasuredSize > 1.0f) ?
		m_BossPreviewCenterY * m_BossPreviewRenderScale :
		(o->BoundingBoxMin[1] + o->BoundingBoxMax[1]) * 0.5f * m_BossPreviewRenderScale;
	const float modelCenterZ = (m_BossPreviewMeasuredSize > 1.0f) ?
		m_BossPreviewCenterZ * m_BossPreviewRenderScale :
		(o->BoundingBoxMin[2] + o->BoundingBoxMax[2]) * 0.5f * m_BossPreviewRenderScale;

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glViewport2(x * g_fScreenRate_x, y * g_fScreenRate_y,
		width * g_fScreenRate_x, height * g_fScreenRate_y);
	gluPerspective2(1.0f, (width * g_fScreenRate_x) / (height * g_fScreenRate_y), 2000, 20000);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	GetOpenGLMatrix(CameraMatrix);
	EnableDepthTest();
	EnableDepthMask();
	glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
	glRotatef(-90.0f, 0.0f, 0.0f, 1.0f);
	// O centro visual calculado acima deve coincidir com o centro do viewport.
	// Os deslocamentos fixos antigos empurravam modelos altos/assimetrico para a borda.
	glTranslatef(-10000.0f, 0.0f, 0.0f);
	glTranslatef(-o->Position[0], -(o->Position[1] + modelCenterY),
		-(o->Position[2] + modelCenterZ));

	Vector(0.0f, 0.0f, 70.0f + sinf((float)timeGetTime() * 0.0015f) * 12.0f, o->Angle);
	o->Scale = m_BossPreviewRenderScale;
	c->HideShadow = true;
	Vector(0.95f, 0.95f, 0.95f, o->Light);

	DisableAlphaBlend();
	EnableDepthTest();
	EnableCullFace();
	EnableDepthMask();
	glDepthFunc(GL_LEQUAL);
	glAlphaFunc(GL_GREATER, 0.25f);
	glDisable(GL_FOG);
	glClear(GL_DEPTH_BUFFER_BIT);
	RenderCharacter(c, o);

	// Mede os vertices que acabaram de ser transformados pelo proprio modelo.
	// Isto fornece o centro visual real mesmo para bosses assimetricos, altos,
	// largos ou com origem deslocada. O valor e aplicado no quadro seguinte.
	if (model != NULL && m_BossPreviewMeasuredSize <= 1.0f)
	{
		vec3_t visualMin;
		vec3_t visualMax;
		Vector(999999.0f, 999999.0f, 999999.0f, visualMin);
		Vector(-999999.0f, -999999.0f, -999999.0f, visualMax);
		bool hasVertices = false;
		for (int meshIndex = 0; meshIndex < model->NumMeshs && meshIndex < MAX_MESH; ++meshIndex)
		{
			Mesh_t* mesh = &model->Meshs[meshIndex];
			for (int vertexIndex = 0; vertexIndex < mesh->NumVertices && vertexIndex < MAX_VERTICES; ++vertexIndex)
			{
				float* vertex = VertexTransform[meshIndex][vertexIndex];
				for (int axis = 0; axis < 3; ++axis)
				{
					visualMin[axis] = min(visualMin[axis], vertex[axis]);
					visualMax[axis] = max(visualMax[axis], vertex[axis]);
				}
				hasVertices = true;
			}
		}

		if (hasVertices)
		{
			// VertexTransform ja contem BodyOrigin (posicao no mundo). Retiramos
			// essa origem para guardar somente o centro local do modelo; caso
			// contrario a posicao seria descontada duas vezes no proximo quadro.
			const float measuredScale = max(0.001f, o->Scale);
			m_BossPreviewCenterY = (((visualMin[1] + visualMax[1]) * 0.5f) -
				model->BodyOrigin[1]) / measuredScale;
			m_BossPreviewCenterZ = (((visualMin[2] + visualMax[2]) * 0.5f) -
				model->BodyOrigin[2]) / measuredScale;
			const float visualWidth = visualMax[1] - visualMin[1];
			const float visualHeight = visualMax[2] - visualMin[2];
			m_BossPreviewMeasuredSize = max(visualWidth, visualHeight) / measuredScale;
		}
	}

	o->Scale = oldScale;
	VectorCopy(oldAngle, o->Angle);
	c->HideShadow = oldHideShadow;
	VectorCopy(oldBodyOrigin, model->BodyOrigin);
	VectorCopy(oldBodyAngle, model->BodyAngle);
	VectorCopy(oldBodyLight, model->BodyLight);
	model->BodyScale = oldBodyScale;
	model->CurrentAction = oldModelAction;

	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glViewport2(0, 0, WindowWidth, WindowHeight);
	EnableAlphaTest();
	DisableDepthTest();
	DisableDepthMask();
	DisableCullFace();
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	FogEnable = oldFog;
	if (oldFog)
		glEnable(GL_FOG);
}

void SEASON3B::CGMInvasionManager::NextInvasion()
{
	// Buscar el iterador que apunta a la clave actual
	auto it = InvasionInfo.find(currentInvasion);

LABEL_REPEAT:
	// Si no existe o ya está al final, no hay siguiente
	if (it == InvasionInfo.end() || std::next(it) == InvasionInfo.end())
	{
		return; // Retornar si no hay siguiente clave
	}

	// Avanzar al siguiente iterador
	++it;

	// Devolver la siguiente clave
	if (it->second.TimeRemaing == 0)
	{
		goto LABEL_REPEAT;
	}

	currentInvasion = it->first;
}

void SEASON3B::CGMInvasionManager::PreviousInvasion()
{
	// Buscar el iterador que apunta a la clave actual
	auto it = InvasionInfo.find(currentInvasion);

LABEL_REPEAT:
	// Si no existe o ya es el primero, no hay clave anterior
	if (it == InvasionInfo.end() || it == InvasionInfo.begin())
	{
		return; // Retornar si no hay clave anterior
	}

	// Retroceder al iterador anterior
	--it;

	if (it->second.TimeRemaing == 0)
	{
		goto LABEL_REPEAT;
	}

	currentInvasion = it->first;
}

INVASION_GLOBAL_INFO* SEASON3B::CGMInvasionManager::FindInvasion(int Index)
{
	// Buscar en el map usando find
	auto it = InvasionInfo.find(Index);

	// Verificar si se encontró el elemento
	if (it != InvasionInfo.end())
	{
		// Devolver un puntero al valor asociado (INVASION_GLOBAL_INFO)
		return &(it->second);
	}

	return NULL;
}
