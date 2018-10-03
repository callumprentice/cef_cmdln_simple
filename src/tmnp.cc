#include <iostream>
#include <ctime>
#include <list>
#include <fstream>

#include "cef_app.h"
#include "cef_client.h"
#include "wrapper/cef_helpers.h"

bool gExitFlag = false;

void writeBMPImage(const std::string& filename,
	unsigned char* pixels,
	int image_width, int image_height)
{
	std::cout << std::endl << "Writing output image (BMP) (" << image_width << " x " << image_height << ") to " << filename << std::endl;

	std::ofstream img_stream(filename.c_str(), std::ios::binary | std::ios::out);
	if (img_stream)
	{
		unsigned char file[14] =
		{
			'B', 'M', 0, 0, 0, 0, 0, 0, 0, 0, 40 + 14, 0, 0, 0
		};
		unsigned char info[40] =
		{
			40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 24, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x13, 0x0B, 0, 0, 0x13, 0x0B, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		};
		int size_data = image_width * image_height * 3;
		int size_all = size_data + sizeof(file) + sizeof(info);

		file[2] = (unsigned char)(size_all);
		file[3] = (unsigned char)(size_all >> 8);
		file[4] = (unsigned char)(size_all >> 16);
		file[5] = (unsigned char)(size_all >> 24);

		info[4] = (unsigned char)(image_width);
		info[5] = (unsigned char)(image_width >> 8);
		info[6] = (unsigned char)(image_width >> 16);
		info[7] = (unsigned char)(image_width >> 24);

		info[8] = (unsigned char)(-image_height);
		info[9] = (unsigned char)(-image_height >> 8);
		info[10] = (unsigned char)(-image_height >> 16);
		info[11] = (unsigned char)(-image_height >> 24);

		info[20] = (unsigned char)(size_data);
		info[21] = (unsigned char)(size_data >> 8);
		info[22] = (unsigned char)(size_data >> 16);
		info[23] = (unsigned char)(size_data >> 24);

		img_stream.write((char*)file, sizeof(file));
		img_stream.write((char*)info, sizeof(info));

		const int image_depth = 4;
		for (int i = 0; i < image_width * image_height * image_depth; i += image_depth)
		{
			const unsigned char red = *(pixels + i + 2);
			const unsigned char green = *(pixels + i + 1);
			const unsigned char blue = *(pixels + i + 0);

			img_stream << blue;
			img_stream << green;
			img_stream << red;
		}

		img_stream.close();
	}
}

class RenderHandler :
	public CefRenderHandler
{
public:
	bool GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override
	{
		int width = 1024;
		int height = 1024;
		rect = CefRect(0, 0, width, height);
		return true;
	}

	void OnPaint(CefRefPtr<CefBrowser> browser,
		PaintElementType type,
		const RectList& dirtyRects,
		const void* buffer,
		int width, int height) override
	{
		std::cout << "OnPaint() for size: " << width << " x " << height << std::endl;

		char* buf = nullptr;
		size_t sz = 0;
		static int imgnum = 1000;
		if (_dupenv_s(&buf, &sz, "HOMEPATH") == 0 && buf != nullptr)
		{
			std::ostringstream ostr;
			ostr << std::string(buf);
			ostr << "/Desktop/saved_page";
			ostr << imgnum++;
			ostr << ".bmp";
			std::cout << "Writing image to " << ostr.str() << std::endl;
			writeBMPImage(ostr.str(), (unsigned char*)buffer, width, height);
			free(buf);
		}
	}

	IMPLEMENT_REFCOUNTING(RenderHandler);
};

class BrowserClient :
	public CefClient,
	public CefLifeSpanHandler
{
public:
	BrowserClient(RenderHandler* render_handler) :
		render_handler_(render_handler)
	{
	}

	CefRefPtr<CefRenderHandler> GetRenderHandler() override
	{
		return render_handler_;
	}

	CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override
	{
		return this;
	}

	void OnAfterCreated(CefRefPtr<CefBrowser> browser) override
	{
		CEF_REQUIRE_UI_THREAD();

		browser_list_.push_back(browser);
	}

	void OnBeforeClose(CefRefPtr<CefBrowser> browser) override
	{
		CEF_REQUIRE_UI_THREAD();

		BrowserList::iterator bit = browser_list_.begin();
		for (; bit != browser_list_.end(); ++bit)
		{
			if ((*bit)->IsSame(browser))
			{
				browser_list_.erase(bit);
				break;
			}
		}

		if (browser_list_.empty())
		{
			gExitFlag = true;
		}
	}

	//bool OnBeforePopup(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
	//	const CefString& target_url, const CefString& target_frame_name,
	//	CefLifeSpanHandler::WindowOpenDisposition target_disposition,
	//	bool user_gesture, const CefPopupFeatures& popupFeatures,
	//	CefWindowInfo& windowInfo, CefRefPtr<CefClient>& client,
	//	CefBrowserSettings& settings, bool* no_javascript_access)
	//{
	//	std::cout << "Page wants to open a popup" << std::endl;

	//	return true;
	//};

	IMPLEMENT_REFCOUNTING(BrowserClient);

private:
	CefRefPtr<CefRenderHandler> render_handler_;
	typedef std::list<CefRefPtr<CefBrowser>> BrowserList;
	BrowserList browser_list_;
};

class CefMinimal : public CefApp
{
public:
	~CefMinimal()
	{
	}

	bool init(int argc, char* argv[])
	{
		CefSettings settings;
		settings.multi_threaded_message_loop = false;

		CefMainArgs args(GetModuleHandle(nullptr));

		if (CefInitialize(args, settings, this, nullptr))
		{
			std::cout << "CefMinimal initialized okay" << std::endl;

			render_handler_ = new RenderHandler();

			browser_client_ = new BrowserClient(render_handler_);

			CefWindowInfo window_info;
			window_info.windowless_rendering_enabled = true;

			CefBrowserSettings browser_settings;
			browser_settings.windowless_frame_rate = 60;

			////////////////////////////////////////////////////////////////////////////////////////
			//
			//  This stanza of code, when introduced, triggers an exception in libcef.dll
			// 
#if 0
			CefRefPtr<CefCookieManager> manager = CefCookieManager::GetGlobalManager(nullptr);
			if (manager)
			{
				bool persist_session_coookies = false;
				std::string cookie_path = ".\\cookies";

				manager->SetStoragePath(cookie_path, persist_session_coookies, nullptr);
			}
#endif
			////////////////////////////////////////////////////////////////////////////////////////

			CefString url = "https://news.google.com";
			browser_ = CefBrowserHost::CreateBrowserSync(window_info, browser_client_.get(), url, browser_settings, nullptr);

			return true;
		}

		std::cout << "Unable to initialize" << std::endl;
		return false;
	}

	void navigate(const std::string url)
	{
		if (browser_.get() && browser_->GetMainFrame())
		{
			std::cout << "CefMinimal loading URL " << url << std::endl;
			browser_->GetMainFrame()->LoadURL(url);
		}
	}

	void update()
	{
		CefDoMessageLoopWork();
	}

	void requestExit()
	{
		if (browser_.get() && browser_->GetHost())
		{
			browser_->GetHost()->CloseBrowser(true);
		}
	}

	void shutdown()
	{
		render_handler_ = nullptr;
		browser_client_ = nullptr;
		browser_ = nullptr;
		CefShutdown();
	}

	IMPLEMENT_REFCOUNTING(CefMinimal);

private:
	CefRefPtr<RenderHandler> render_handler_;
	CefRefPtr<BrowserClient> browser_client_;
	CefRefPtr<CefBrowser> browser_;
};

int main(int argc, char* argv[])
{
	CefMainArgs main_args(GetModuleHandle(nullptr));

	int exit_code = CefExecuteProcess(main_args, nullptr, nullptr);
	if (exit_code >= 0)
	{
		return exit_code;
	}

	CefRefPtr<CefMinimal> cm = new CefMinimal();

	cm->init(argc, argv);

	time_t start_time;
	time(&start_time);

	MSG msg;
	do
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			cm->update();

			if (gExitFlag == false)
			{
				if (time(nullptr) > start_time + 5)
				{
					cm->requestExit();
				}
			}
			else
			{
				PostQuitMessage(0);
			}
		}

	} while (msg.message != WM_QUIT);

	cm->shutdown();

	cm = nullptr;

	return 0;
}
