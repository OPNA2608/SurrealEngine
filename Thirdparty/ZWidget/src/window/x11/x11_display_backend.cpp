
#include "x11_display_backend.h"
#include "x11_display_window.h"

#ifdef USE_DBUS
#include "window/dbus/x11_open_file_dialog.h"
#include "window/dbus/x11_save_file_dialog.h"
#include "window/dbus/x11_open_folder_dialog.h"
#endif

std::unique_ptr<DisplayWindow> X11DisplayBackend::Create(DisplayWindowHost* windowHost, bool popupWindow, DisplayWindow* owner)
{
	return std::make_unique<X11DisplayWindow>(windowHost, popupWindow, static_cast<X11DisplayWindow*>(owner));
}

void X11DisplayBackend::ProcessEvents()
{
	X11DisplayWindow::ProcessEvents();
}

void X11DisplayBackend::RunLoop()
{
	X11DisplayWindow::RunLoop();
}

void X11DisplayBackend::ExitLoop()
{
	X11DisplayWindow::ExitLoop();
}

Size X11DisplayBackend::GetScreenSize()
{
	return X11DisplayWindow::GetScreenSize();
}

void* X11DisplayBackend::StartTimer(int timeoutMilliseconds, std::function<void()> onTimer)
{
	return X11DisplayWindow::StartTimer(timeoutMilliseconds, std::move(onTimer));
}

void X11DisplayBackend::StopTimer(void* timerID)
{
	X11DisplayWindow::StopTimer(timerID);
}

#ifdef USE_DBUS
std::unique_ptr<OpenFileDialog> X11DisplayBackend::CreateOpenFileDialog(Widget* owner)
{
	std::string ownerHandle;
	if (owner)
		ownerHandle = "x11:" + std::to_string(reinterpret_cast<unsigned long>(owner->GetNativeHandle()));
	return std::make_unique<DBusOpenFileDialog>(ownerHandle);
}

std::unique_ptr<SaveFileDialog> X11DisplayBackend::CreateSaveFileDialog(Widget* owner)
{
	std::string ownerHandle;
	if (owner)
		ownerHandle = "x11:" + std::to_string(reinterpret_cast<unsigned long>(owner->GetNativeHandle()));
	return std::make_unique<DBusSaveFileDialog>(ownerHandle);
}

std::unique_ptr<OpenFolderDialog> X11DisplayBackend::CreateOpenFolderDialog(Widget* owner)
{
	std::string ownerHandle;
	if (owner)
		ownerHandle = "x11:" + std::to_string(reinterpret_cast<unsigned long>(owner->GetNativeHandle()));
	return std::make_unique<DBusOpenFolderDialog>(ownerHandle);
}
#endif
