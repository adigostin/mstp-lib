
#include "pch.h"
#include "SimulatorInterfaces.h"

using namespace std;

class EditArea : public IEditArea
{
	HWND _hwnd;

public:
	EditArea(IProject* project, IProjectWindow* pw, const RECT& rect, ID3D11DeviceContext1* deviceContext, IDWriteFactory* dWriteFactory, IWICImagingFactory2* wicFactory)
	{
	}

	virtual ~EditArea()
	{
		::DestroyWindow (_hwnd);
	}

	virtual HWND GetHWnd() const override final { return _hwnd; }
};

extern EditAreaFactory* const editAreaFactory = [](auto... params) { return unique_ptr<IEditArea>(new EditArea(params...)); };
