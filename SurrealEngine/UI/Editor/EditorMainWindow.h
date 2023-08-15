
#pragma once

#include "UI/Widgets/MainWindow/MainWindow.h"

class EditorWorkspace;

class EditorMainWindow : public MainWindow
{
public:
	EditorMainWindow();
	~EditorMainWindow();

	EditorWorkspace* Workspace = nullptr;

	bool ExitFlag = false;
	void OnClose() override { ExitFlag = true; }
};