#pragma once

// Reusable title-bar dragging for the classic NewUI windows.
// The owner must provide SetPos(x, y). Each window class has its own state.
namespace SEASON3B
{
	template <class T>
	bool ProcessWindowDrag(T* owner, int posX, int posY, int width, int height,
		int titleHeight = 24, int titleRightPadding = 30)
	{
		static bool dragging = false;
		static int offsetX = 0;
		static int offsetY = 0;

		int draggableWidth = width - titleRightPadding;
		if (draggableWidth < 1)
			draggableWidth = width;

		if (!dragging && IsPress(VK_LBUTTON) &&
			CheckMouseIn(posX, posY, draggableWidth, titleHeight))
		{
			dragging = true;
			offsetX = MouseX - posX;
			offsetY = MouseY - posY;
		}

		if (!dragging)
			return false;

		if (!MouseLButton)
		{
			dragging = false;
			return false;
		}

		int newX = MouseX - offsetX;
		int newY = MouseY - offsetY;
		owner->SetPos(newX, newY);
		return true;
	}
}
