#include <stdio.h>

#include <Application.h>
#include <Window.h>
#include <View.h>
#include "Lists.h"
#include "AppKitPtrs.h"
#include "../Locks/Timer.h"

#include "Domains.h"


AppKitPtrs::ExternalPtr<class TestView> gView;


class RectObj: public Lists::List
{
private:
	BView *fView;
	BRect fFrame;
	rgb_color fColor;
	friend class TestView;

public:
	RectObj(BRect frame, rgb_color color): fView(NULL), fFrame(frame), fColor(color) {}
	
	void LockLooper()
	{
		if (fView != NULL) fView->LockLooper();
	}
	
	void UnlockLooper()
	{
		if (fView != NULL) fView->UnlockLooper();
	}

	BRect Frame() const {return fFrame;}
	rgb_color Color() const {return fColor;}
	
	void SetFrame(BRect frame)
	{
		if (fView != NULL) fView->Invalidate(fFrame);
		fFrame = frame;
		if (fView != NULL) fView->Invalidate(fFrame);
	}
	
	void SetColor(rgb_color color)
	{
		fColor = color;
		if (fView != NULL) fView->Invalidate(fFrame);
	}
	
};

class TestView: public BView
{
private:
	Lists::List fObjs;

public:
	TestView(BRect frame, const char *name):
		BView(frame, name, B_FOLLOW_NONE, B_FULL_UPDATE_ON_RESIZE | B_WILL_DRAW | B_SUBPIXEL_PRECISE),
		fObjs({&fObjs, &fObjs})
	{
	}
	
	~TestView()
	{
		while (fObjs.fNext != &fObjs) {
			RectObj *it = (RectObj*)fObjs.fNext;
			fObjs.Remove(it);
		}
	}
	
	void Attach(RectObj *obj)
	{
		Assert(obj->fView == NULL);
		fObjs.InsertBefore(obj);
		obj->fView = this;
		Invalidate(obj->Frame());
	}
	
	void Detach(RectObj *obj)
	{
		Assert(obj->fView == this);
		obj->fView = NULL;
		Invalidate(obj->Frame());
		fObjs.Remove(obj);
	}

	void Draw(BRect dirty)
	{
		for (RectObj *it = (RectObj*)fObjs.fNext; it != &fObjs; it = (RectObj*)it->fNext) {
			SetHighColor(it->Color());
			FillRect(it->Frame());
		}
	}
	
};

class TestWindow: public BWindow
{
private:
	TestView *fView;
public:
	TestWindow(BRect frame): BWindow(frame, "TestApp", B_TITLED_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL, B_ASYNCHRONOUS_CONTROLS)
	{
		fView = new TestView(frame.OffsetToCopy(B_ORIGIN), "view");
		fView->SetResizingMode(B_FOLLOW_ALL);
		AddChild(fView, NULL);
		gView = fView;
/*
		for (int y = 0; y < 16; y++)
			for (int x = 0; x < 16; x++)
				fView->Attach(new RectObj(BRect(x*8, y*8, (x + 1)*8 - 2, (y + 1)*8 - 2), make_color(0, 0, 0)));
*/
	}
	
	~TestWindow()
	{
		RemoveChild(fView);
	}

	void Quit()
	{
		be_app_messenger.SendMessage(B_QUIT_REQUESTED);
		BWindow::Quit();
	}

};

class TestObject0: public Object
{
};

class TestObject: public Object
{
private:
	BPoint fOrg;
	ExternalPtr<TestObject0> fTestObj;
public:
	TestObject(BPoint org, ExternalPtr<TestObject0> testObj): fOrg(org), fTestObj(testObj) {}
	
	void Do()
	{
		auto obj = AppKitPtrs::MakeExternal<RectObj>(BRect(0, 0, 7, 7).OffsetByCopy(fOrg), make_color(0, 0, 0));
		gView->Attach(obj.Get());
		for (;;) {
			auto testObjLock = fTestObj.Lock();
			for (int i = 0; i < 128; i++) {
				obj->SetFrame(obj->Frame().OffsetByCopy(1, 0));
				snooze(10000);
			}
			for (int i = 0; i < 128; i++) {
				obj->SetFrame(obj->Frame().OffsetByCopy(-1, 0));
				snooze(10000);
			}
			CurrentDomain()->Yield();
		}
	}
};

class TestObject2: public Object
{
private:
	BPoint fOrg;
	ExternalPtr<TestObject0> fTestObj;
	AppKitPtrs::ExternalPtr<RectObj> obj;
	int i, j;
	
	class _Timer: public Timer {
	private:
		TestObject2 *fBase;
	public:
		_Timer(TestObject2 *base): fBase(base) {}

		void Do() override
		{
			MakeAsyncRequestMth(ExternalPtr<TestObject2>(fBase), &TestObject2::Do)->Schedule();
		}
	} fTimer;

public:
	TestObject2(BPoint org, ExternalPtr<TestObject0> testObj): fOrg(org), fTestObj(testObj), i(0), j(0), fTimer(this) {
		obj = AppKitPtrs::MakeExternal<RectObj>(BRect(0, 0, 7, 7).OffsetByCopy(fOrg), make_color(0, 0, 0));
		gView->Attach(obj.Get());
	}
	
	void Do()
	{
		gTimerRoster.Schedule(&fTimer, system_time() + 1000000/1000);
		//CurrentDomain()->Schedule(CurrentDomain()->RootRequest());
		//auto testObjLock = fTestObj.Lock();
		if (j == 0) {
			obj->SetFrame(obj->Frame().OffsetByCopy(1, 0));
		} else {
			obj->SetFrame(obj->Frame().OffsetByCopy(-1, 0));
		}
		//snooze(100);
		i++;
		if (i >= 128) {
			i = 0;
			j = (j + 1) % 2;
		}
	}
};

class TestApplication: public BApplication
{
private:
	TestWindow *fWnd;
	
public:
	TestApplication(): BApplication("application/x-vnd.Test-App")
	{
		fWnd = new TestWindow(BRect(0, 0, 256, 256).OffsetByCopy(64, 64));
		fWnd->Show();
		
		for (int j = 0; j < 2; j++) {
			auto p0 = MakeExternal<TestObject0>();
	/*
			for (int i = 0; i < 8; i++) {
				auto p1 = MakeExternal<TestObject>(BPoint(8, 8 + 12*(i + 8*j)), p0);
				MakeAsyncRequestMth(p1, &TestObject::Do)->Schedule();
			}
	*/
			for (int i = 0; i < 8; i++) {
				//auto p0Locked = p0.Switch();
				DomainSection ds(p0.GetDomain());
				auto p1 = new TestObject2(BPoint(8, 8 + 12*(i + 8*j)), p0);
				MakeAsyncRequestMth(ExternalPtr<TestObject2>(p1), &TestObject2::Do)->Schedule();
			}
		}
	}

};


int main()
{
	TestApplication app;
	app.Run();
	return 0;
}
