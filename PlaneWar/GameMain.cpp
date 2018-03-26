#include <windows.h>
#include <cstdlib>
#include <tchar.h>
#include <gdiplus.h>
#include <mmsystem.h>
#include <ctime>
#include "resource.h"

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "winmm.lib")

ULONG_PTR gdiplusStartupToken;
using namespace Gdiplus;

HINSTANCE hInst;
const int iWindowWidth = 480;
const int iWindowHeight = 852;

enum GameState { GS_Menu, GS_Playing, GS_Result };
GameState gameState = GS_Menu;
HDC g_hdc;
HDC g_mdc;
HDC g_ndc;


void GameStart(HWND);
void GameUpdate(HWND);
void GameEnd(HWND);



LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	GdiplusStartupInput gdiInput;
	GdiplusStartup(&gdiplusStartupToken, &gdiInput, NULL);
	hInst = hInstance;
	static TCHAR szWindowClass[] = _T("PlaneWar");
	static TCHAR szTitle[] = _T("飞机大战");
	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(hInstance, NULL);
	if (!RegisterClassEx(&wcex)) {
		MessageBox(NULL, _T("调用RegisterClassEx失败!"), _T("提示"), MB_ICONWARNING);
		return 1;
	}

	HWND hWnd = CreateWindow(szWindowClass, szTitle, WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT, iWindowWidth + 16, iWindowHeight + 38, NULL, NULL, hInstance, NULL);
	if (!hWnd) {
		MessageBox(NULL, _T("调用CreateWindow失败!"), _T("提示"), MB_ICONWARNING);
		return 1;
	}

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	g_hdc = GetDC(hWnd);
	g_mdc = CreateCompatibleDC(g_hdc);
	g_ndc = CreateCompatibleDC(g_hdc);


	HBITMAP whiteBmpOne = CreateCompatibleBitmap(g_hdc, iWindowWidth, iWindowHeight);
	HBITMAP whiteBmpTwo = CreateCompatibleBitmap(g_hdc, iWindowWidth, iWindowHeight);

	SelectObject(g_mdc, whiteBmpOne);
	SelectObject(g_ndc, whiteBmpTwo);

	GameStart(hWnd);

	DWORD tNow = GetTickCount();
	DWORD tPre = GetTickCount();

	MSG msg = { 0 };

	while (msg.message != WM_QUIT) {
		if (PeekMessage(&msg, NULL, NULL, NULL, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {
			tNow = GetTickCount();
			if (tNow - tPre > 10) {
				GameUpdate(hWnd);
				tPre = tNow;
			}
		}
	}
	GameEnd(hWnd);
	GdiplusShutdown(gdiplusStartupToken);
	return (int)msg.wParam;
}

struct Bullet {
	int x;
	int y;
	const int iWidth = 9;
	const int iHeight = 21;
	bool isExist = false;
};

struct Enemy {
	int x;
	int y;
	const int iWidth = 51;
	const int iHeight = 39;
	bool isExist = false;
	bool isDie = false;
	bool isDodge;
	int iDieAnimationIndex = 0;
	int iDieAnimationTimer = 0;
};

struct GameMenu {
	int iLoadingTimer = 0;
	int iLoadingIndex = 0;
	void Start(HWND hWnd) {
		mciSendString(_T("open sound\\game_start.mp3"), NULL, NULL, NULL);
		mciSendString(_T("play sound\\game_start.mp3 repeat"), NULL, NULL, NULL);
	}
	void Update(HWND hWnd) {
		Graphics graphics(g_mdc);
		Image imgBackground(_T("image\\background.png"));
		Image imgName(_T("image\\name.png"));
		graphics.DrawImage(&imgBackground, 0, 0);
		graphics.DrawImage(&imgName, 20, 50);
		iLoadingTimer++;
		if (iLoadingTimer >= 10) {
			iLoadingIndex++;
			iLoadingIndex %= 4;
			iLoadingTimer = 0;
		}
		Image imgGameloading[4] = { _T("image\\game_loading1.png"), _T("image\\game_loading2.png"), _T("image\\game_loading3.png"), _T("image\\game_loading4.png") };
		graphics.DrawImage(&imgGameloading[iLoadingIndex], 150, 600);
		BitBlt(g_hdc, 0, 0, iWindowWidth, iWindowHeight, g_mdc, 0, 0, SRCCOPY);
	}
	void OnWindowMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
		switch (message) {
		case WM_LBUTTONUP:
			mciSendString(_T("stop sound\\game_start.mp3"), NULL, NULL, NULL);
			gameState = GS_Playing;
			GameStart(hWnd);
			break;
		}

	}
};

struct GamePlaying {
	int iBackgroundOffset = 0;

	int iHeroIndex = 0;
	int iHeroTimer = 0;

	int iPlayerPositionX = 180;
	int iPlayerPositionY = 680;

	Bullet bulletArray[30];
	int iBulletTimer = 0;

	bool bIsMouseDown = false;
	POINT pPreMousePoint;

	Enemy enemyArray[30];
	int iEnemySpawnTimer = 0;

	void Start(HWND hWnd) {
		mciSendString(_T("open sound\\game_music.mp3"), NULL, NULL, NULL);
		mciSendString(_T("play sound\\game_music.mp3 repeat"), NULL, NULL, NULL);
		srand((unsigned)time(NULL));
	}

	void Update(HWND hWnd) {
		//绘制移动背景
		iBackgroundOffset += 5;
		Graphics graphics(g_mdc);
		Image imgBackground(_T("image\\background.png"));
		graphics.DrawImage(&imgBackground, 0, iBackgroundOffset, iWindowWidth, iWindowHeight);
		graphics.DrawImage(&imgBackground, 0, -(iWindowHeight - iBackgroundOffset), iWindowWidth, iWindowHeight);
		BitBlt(g_ndc, 0, 0, iWindowWidth, iWindowHeight, g_mdc, 0, 0, SRCCOPY);
		BitBlt(g_ndc, 0, 0, iWindowWidth, iWindowHeight, g_mdc, 0, 0, SRCCOPY);
		if (iBackgroundOffset > iWindowHeight) {
			iBackgroundOffset -= iWindowHeight;
		}
		//绘制战机
		iHeroTimer++;
		if (iHeroTimer >= 10) {
			iHeroIndex++;
			iHeroIndex %= 2;
			iHeroTimer = 0;
		}
		Image imgHero[2] = { _T("image\\hero1.png"), _T("image\\hero2.png") };
		graphics.DrawImage(&imgHero[iHeroIndex], iPlayerPositionX, iPlayerPositionY);
		BitBlt(g_ndc, 0, 0, iWindowWidth, iWindowHeight, g_mdc, 0, 0, SRCCOPY);
		//发射子弹
		iBulletTimer++;
		if (iBulletTimer % 20 == 0) {
			for (int i = 0; i < 30; ++i) {
				if (bulletArray[i].isExist == false) {
					bulletArray[i].x = iPlayerPositionX + 46;
					bulletArray[i].y = iPlayerPositionY - 21;
					bulletArray[i].isExist = true;
					iBulletTimer = 0;
					break;
				}
			}
		}
		Image imgBullet1 = _T("image\\bullet1.png");
		for (int i = 0; i < 30; ++i)
		{
			if (bulletArray[i].isExist)
			{
				bulletArray[i].y -= 3;
				if (bulletArray[i].y < -21)
				{
					bulletArray[i].isExist = false;
					continue;
				}
				graphics.DrawImage(&imgBullet1, bulletArray[i].x, bulletArray[i].y);
				BitBlt(g_ndc, 0, 0, iWindowWidth, iWindowHeight, g_mdc, 0, 0, SRCCOPY);
			}
		}
		//移动战机
		if (bIsMouseDown) {
			POINT pNowMousePoint;
			GetCursorPos(&pNowMousePoint);
			int xOffset = pNowMousePoint.x - pPreMousePoint.x;
			int yOffset = pNowMousePoint.y - pPreMousePoint.y;
			iPlayerPositionX += xOffset;
			iPlayerPositionY += yOffset;
			pPreMousePoint = pNowMousePoint;
			if (iPlayerPositionX < -50) {
				iPlayerPositionX = -50;
			}
			if (iPlayerPositionX > iWindowWidth - 50) {
				iPlayerPositionX = iWindowWidth - 50;
			}
			if (iPlayerPositionY < 0) {
				iPlayerPositionY = 0;
			}
			if (iPlayerPositionY > 730) {
				iPlayerPositionY = 730;
			}
		}

		//生成敌机
		iEnemySpawnTimer++;
		if (iEnemySpawnTimer % 25 == 0) {
			for (int i = 0; i < 30; ++i) {
				if (enemyArray[i].isExist == false) {
					enemyArray[i].x = rand() % (iWindowWidth - 25);
					enemyArray[i].y = -39;
					enemyArray[i].isDodge = bool(rand() % 2);
					enemyArray[i].isExist = true;
					enemyArray[i].isDie = false;
					enemyArray[i].iDieAnimationTimer = 0;
					enemyArray[i].iDieAnimationIndex = 0;
					iEnemySpawnTimer = 0;
					break;
				}
			}
		}
		for (int i = 0; i < 30; ++i) {
			if (enemyArray[i].isExist && !enemyArray[i].isDie) {
				enemyArray[i].y += 5;
				if (enemyArray[i].x <= -25) {
					enemyArray[i].isDodge = true;
				}
				if (enemyArray[i].x >= iWindowWidth - 25) {
					enemyArray[i].isDodge = false;
				}
				if (enemyArray[i].isDodge) {
					enemyArray[i].x += 2;
				}
				else {
					enemyArray[i].x -= 2;
				}
				if (enemyArray[i].y > iWindowHeight) {
					enemyArray[i].isExist = false;
				}
			}
		}
		Image imgEnemy[5] = { _T("image\\enemy0.png"), _T("image\\enemy0_down1.png"), _T("image\\enemy0_down2.png"), _T("image\\enemy0_down3.png"), _T("image\\enemy0_down4.png") };
		for (int i = 0; i < 30; ++i) {
			if (enemyArray[i].isExist) {
				if (enemyArray[i].isDie) {
					enemyArray[i].iDieAnimationTimer++;
					enemyArray[i].iDieAnimationIndex = enemyArray[i].iDieAnimationTimer / 8;
					if (enemyArray[i].iDieAnimationIndex > 4)
					{
						enemyArray[i].isExist = false;
						continue;
					}
					graphics.DrawImage(&imgEnemy[enemyArray[i].iDieAnimationIndex], enemyArray[i].x, enemyArray[i].y);
					BitBlt(g_ndc, 0, 0, iWindowWidth, iWindowHeight, g_mdc, 0, 0, SRCCOPY);
				}
				else
				{
					graphics.DrawImage(&imgEnemy[0], enemyArray[i].x, enemyArray[i].y);
					BitBlt(g_ndc, 0, 0, iWindowWidth, iWindowHeight, g_mdc, 0, 0, SRCCOPY);
				}
			}
		}

		for (int i = 0; i < 30; ++i) {
			if (enemyArray[i].isExist && !enemyArray[i].isDie) {
				for (int j = 0; j < 30; ++j) {
					if (bulletArray[j].isExist) {
						if (IsInclude(enemyArray[i].x, enemyArray[i].y, enemyArray[i].iWidth, enemyArray[i].iHeight, bulletArray[j].x, bulletArray[j].y, bulletArray[j].iWidth, bulletArray[j].iHeight)) {
							bulletArray[j].isExist = false;
							enemyArray[i].isDie = true;
							PlaySound(_T("sound/enemy0_down.wav"), NULL, SND_FILENAME | SND_ASYNC);
						}
					}
				}
			}

			if (enemyArray[i].isExist && !enemyArray[i].isDie) {
				if (IsInclude(enemyArray[i].x, enemyArray[i].y, enemyArray[i].iWidth, enemyArray[i].iHeight, iPlayerPositionX, iPlayerPositionY, 100, 98)) {
					mciSendString(_T("stop sound\\game_music.mp3"), NULL, NULL, NULL);
					PlaySound(_T("sound/game_over.wav"), NULL, SND_FILENAME | SND_ASYNC);
					gameState = GS_Result;
					GameStart(hWnd);
				}
			}
		}

		
		BitBlt(g_hdc, 0, 0, iWindowWidth, iWindowHeight, g_ndc, 0, 0, SRCCOPY);
	}
	bool IsInclude(int ax, int ay, int aWidth, int aHeigh, int bx, int by, int bWidth, int bHeigh) {
		if ((abs(ax + (aWidth / 2) - (bx + (bWidth / 2))) <= (aWidth / 2 + (bWidth / 2))) && (abs(ay + (aHeigh / 2) - (by + (bHeigh / 2))) <= (aHeigh / 2 + (bHeigh / 2)))) {
			return true;
		}
		return false;
	}
	void OnWindowMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
		switch (message) {
		case WM_LBUTTONDOWN:
			bIsMouseDown = true;
			GetCursorPos(&pPreMousePoint);
			break;
		case WM_LBUTTONUP:
			bIsMouseDown = false;
			break;
		}
	}
};

struct GameResult {
	void Start(HWND hWnd) {
		mciSendString(_T("open sound\\thedawn.mp3"), NULL, NULL, NULL);
		mciSendString(_T("play sound\\thedawn.mp3 repeat"), NULL, NULL, NULL);
	}
	void Update(HWND hWnd) {
		Graphics graphics(g_hdc);
		Image imgGameover(_T("image\\gameover.png"));
		graphics.DrawImage(&imgGameover, 0, 0);
	}
	void OnWindowMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {

	}
};

GameMenu gameMenu;
GamePlaying gamePlaying;
GameResult gameResult;

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	PAINTSTRUCT ps;
	HDC hdc;
	switch (message) {
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
		switch (gameState) {
		case GS_Menu:
			gameMenu.OnWindowMessage(hWnd, message, wParam, lParam);
			break;
		case GS_Playing:
			gamePlaying.OnWindowMessage(hWnd, message, wParam, lParam);
			break;
		case GS_Result:
			gameResult.OnWindowMessage(hWnd, message, wParam, lParam);
			break;
		}
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);

		EndPaint(hWnd, &ps);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
		break;
	}
	return 0;
}

void GameStart(HWND hWnd) {
	switch (gameState) {
	case GS_Menu:
		gameMenu.Start(hWnd);
		break;
	case GS_Playing:
		gamePlaying.Start(hWnd);
		break;
	case GS_Result:
		gameResult.Start(hWnd);
		break;
	}
}

void GameUpdate(HWND hWnd) {
	switch (gameState) {
	case GS_Menu:
		gameMenu.Update(hWnd);
		break;
	case GS_Playing:
		gamePlaying.Update(hWnd);
		break;
	case GS_Result:
		gameResult.Update(hWnd);
		break;
	}
}

void GameEnd(HWND hWnd) {
	DeleteDC(g_mdc);
	DeleteDC(g_ndc);
	ReleaseDC(hWnd, g_hdc);
}