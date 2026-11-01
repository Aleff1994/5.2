#pragma once

class CBuildProfileUI
{
public:
	CBuildProfileUI();

	static CBuildProfileUI* Instance();
	void Draw();

private:
	bool Button(float x, float y, float width, float height, const char* text, bool selected = false, bool textured = false);
	void Execute(const char* action);

	bool m_Open;
	int m_Selected;
	DWORD m_LastClick;
	bool m_TextureAttempted;
	bool m_TextureLoaded;
	bool m_IconTextureAttempted;
	float m_IconX, m_IconY, m_DragOffsetX, m_DragOffsetY;
	bool m_IconPositioned, m_IconDragging, m_IconMoved;
	enum { BITMAP_BUILD_PROFILE_PANEL = 35990, BITMAP_BUILD_PROFILE_ICON = 35991 };
};

#define gBuildProfileUI (CBuildProfileUI::Instance())
