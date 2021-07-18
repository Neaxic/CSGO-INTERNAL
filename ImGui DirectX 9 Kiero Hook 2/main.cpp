#include "includes.h"

#ifdef _WIN64
#define GWL_WNDPROC GWLP_WNDPROC
#endif

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

EndScene oEndScene = NULL;
WNDPROC oWndProc;
static HWND window = NULL;

void InitImGui(LPDIRECT3DDEVICE9 pDevice)
{
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;
	ImGui_ImplWin32_Init(window);
	ImGui_ImplDX9_Init(pDevice);
}

struct Vector2 { float x, y; };
struct Vector3 { float x, y, z; };
struct Vector4 { float x, y, z, w; };
struct ColorStr {
	int r, g, b, a;
};
struct BoneMatrix_t {
	byte pad1[12];
	float x;
	byte pad2[12];
	float y;
	byte pad3[12];
	float z;
};

bool init = false;
bool show_main = false;

int screenX = GetSystemMetrics(SM_CXSCREEN);
int screenY = GetSystemMetrics(SM_CYSCREEN);

bool esp = false;
float boxwidth = 0.5;

ColorStr BoxColor;
int boxThickness = 2;
int backingAlpha = 35;
bool drawBacking = true;

bool ThirdPerson = false;
bool bhop = false;
bool glow = false;
bool DrawWatermark = true;

bool antialias_all = true;

ImColor EnemyColor;
ImColor TeamColor;

bool triggerbot = false;
bool triggerbotRandom = false;
int triggerCustomDelay = 0;

DWORD LocalPlayer = NULL;
DWORD gameMod = NULL;
DWORD engineMod = NULL;

bool WordToScreen(Vector3 pos, Vector2& screen, float matrix[16], int width, int height) {
	Vector4 clipCoords;
	clipCoords.x = pos.x * matrix[0] + pos.y * matrix[1] + pos.z * matrix[2] + matrix[3];
	clipCoords.y = pos.x * matrix[4] + pos.y * matrix[5] + pos.z * matrix[6] + matrix[7];
	clipCoords.z = pos.x * matrix[8] + pos.y * matrix[9] + pos.z * matrix[10] + matrix[11];
	clipCoords.w = pos.x * matrix[12] + pos.y * matrix[13] + pos.z * matrix[14] + matrix[15];

	if (clipCoords.w < 0.1f) {
		return false;
	}

	Vector3 NDC;
	NDC.x = clipCoords.x / clipCoords.w;
	NDC.y = clipCoords.y / clipCoords.w;
	NDC.z = clipCoords.z / clipCoords.w;

	screen.x = (width / 2 * NDC.x) + (NDC.x + width / 2);
	screen.y = -(height / 2 * NDC.y) + (NDC.y + height / 2);
	return true;
}
Vector2 GetBonePosition(uintptr_t Entity, int bone) {
	uintptr_t BoneMatrix_Base = *(uintptr_t*)(Entity + m_dwBoneMatrix);
	BoneMatrix_t Bone = *(BoneMatrix_t*)(BoneMatrix_Base + sizeof(Bone) * bone);
	Vector3 Location = { Bone.x, Bone.y, Bone.z };
	Vector2 ScreenCords;

	float vMatrix[16];
	memcpy(&vMatrix, (PBYTE*)(gameMod + dwViewMatrix), sizeof(vMatrix));
	if (WordToScreen(Location, ScreenCords, vMatrix, screenX, screenY)) {
		return ScreenCords;
	}
	return { 0, 0 };
}

void HackInit() {
	gameMod = (uintptr_t)GetModuleHandle("client.dll");
	engineMod = (uintptr_t)GetModuleHandle("engine.dll");

	LocalPlayer = *(uintptr_t*)(gameMod + dwLocalPlayer);

	TeamColor.Value.w = 1;
	EnemyColor.Value.w = 1;
}

void DrawESPBox(int x, int y, int w, int h, int thickness, bool antialias, D3DCOLOR col) {
	DrawLine(x, y, x + w, y, thickness, antialias, col);
	DrawLine(x, y + h, x + w, y + h, thickness, antialias, col);
	DrawLine(x, y, x, y + h, thickness, antialias, col);
	DrawLine(x + w, y, x + w, y + h, thickness, antialias, col);
}

long __stdcall hkEndScene(LPDIRECT3DDEVICE9 pDevice)
{
	if (!init)
	{
		HackInit();
		InitImGui(pDevice);
		init = true;
	}

	if (GetAsyncKeyState(VK_INSERT) & 1) {
		show_main = !show_main;
	}

	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ShowWatermark(&DrawWatermark, "PØLSE TIME | sut en pik", ImVec4(255, 255, 255, 255));

	if (show_main) {
		ImGui::Begin("ImGui Window");
		ImGui::Checkbox("ESp", &esp);
		ImGui::Checkbox("Bhop", &bhop);
		ImGui::Text("Test");

		ImGui::Checkbox("triggerbot", &triggerbot);
		ImGui::Checkbox("triggerbotRandom", &triggerbotRandom);
		ImGui::SliderInt("triggerCustomDelay", &triggerCustomDelay, 0, 250);
		
		ImGui::Checkbox("ThirdPerson", &ThirdPerson);
		ImGui::Checkbox("DirextX Antialiasing", &antialias_all);
		
		if (esp) {
			ImGui::SliderFloat("box width", &boxwidth, 0.00f, 1.00f);
			ImGui::SliderInt("box Thickness", &boxThickness, 1, 10);
			ImGui::Text("Colors");
			ImGui::SliderInt("R", &BoxColor.r, 0, 255);
			ImGui::SliderInt("G", &BoxColor.g, 0, 255);
			ImGui::SliderInt("B", &BoxColor.b, 0, 255);
			ImGui::SliderInt("A", &BoxColor.a, 0, 255);
		}

		ImGui::Checkbox("GLOW", &glow);
		if (glow) {
			ImGui::ColorPicker4("Enemy", (float*)&EnemyColor);
			ImGui::ColorPicker4("Friendly", (float*)&TeamColor);
		}

		ImGui::End();
	}
	

	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());


	if (esp) {
		while(LocalPlayer == NULL) {
			LocalPlayer = *(DWORD*)(gameMod + dwLocalPlayer);
		}

		Vector2 ScreenPosition; 
		Vector2 HeadPosition;
		for (int i = 0; i < 32; i++) {
			uintptr_t Entity = *(uintptr_t*)(gameMod + dwEntityList + (i * 0x10));
			if (Entity != NULL) {
				Vector3 EntityLocation = *(Vector3*)(Entity + m_vecOrigin);
				float vMatrix[16];
				memcpy(&vMatrix, (PBYTE*)(gameMod + dwViewMatrix), sizeof(vMatrix));
				if (WordToScreen(EntityLocation, ScreenPosition, vMatrix, screenX, screenY)) {
					uintptr_t BoneMatrix_Base = *(uintptr_t*)(Entity + m_dwBoneMatrix);
					BoneMatrix_t Bone = *(BoneMatrix_t*)(BoneMatrix_Base + sizeof(Bone) * 9);
					Vector3 Location = { Bone.x, Bone.y, Bone.z + 10 };
					Vector2 ScreenCords;

					float vMatrix[16];
					memcpy(&vMatrix, (PBYTE*)(gameMod + dwViewMatrix), sizeof(vMatrix));
					if (WordToScreen(Location, ScreenCords, vMatrix, screenX, screenY)) {
						HeadPosition = ScreenCords;
						DrawESPBox(ScreenPosition.x - (((ScreenPosition.y - HeadPosition.y) * boxwidth) / 2),
							HeadPosition.y,
							(ScreenPosition.y - HeadPosition.y) * boxwidth,
							ScreenPosition.y - HeadPosition.y,
							boxThickness,
							antialias_all,
							D3DCOLOR_ARGB(BoxColor.a, BoxColor.r, BoxColor.g, BoxColor.b));
					}

					HeadPosition = GetBonePosition(Entity, 9);
					HeadPosition.z
				}
			}
		}
	}

	return oEndScene(pDevice);
}

LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

	if (true && ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
		return true;

	return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

BOOL CALLBACK EnumWindowsCallback(HWND handle, LPARAM lParam)
{
	DWORD wndProcId;
	GetWindowThreadProcessId(handle, &wndProcId);

	if (GetCurrentProcessId() != wndProcId)
		return TRUE; // skip to next window

	window = handle;
	return FALSE; // window found abort search
}

HWND GetProcessWindow()
{
	window = NULL;
	EnumWindows(EnumWindowsCallback, NULL);
	return window;
}

DWORD WINAPI MainThread(LPVOID lpReserved)
{
	bool attached = false;
	do
	{
		if (kiero::init(kiero::RenderType::D3D9) == kiero::Status::Success)
		{
			kiero::bind(42, (void**)& oEndScene, hkEndScene);
			do
				window = GetProcessWindow();
			while (window == NULL);
			oWndProc = (WNDPROC)SetWindowLongPtr(window, GWL_WNDPROC, (LONG_PTR)WndProc);
			attached = true;
		}
	} while (!attached);
	return TRUE;
}

DWORD WINAPI ThirdPersonThread(LPVOID lp) {
	DWORD gameMod = (DWORD)GetModuleHandle("client.dll");

	while (LocalPlayer == NULL) {
		LocalPlayer = *(DWORD*)(gameMod + dwLocalPlayer);
	}

	while (true) {
		if (ThirdPerson) {
			*(int*)(LocalPlayer + m_iObserverMode) = 1;
		}
		else {
			*(int*)(LocalPlayer + m_iObserverMode) = 0;
		}
	}
}

DWORD WINAPI TriggerThread(LPVOID lp) {
	DWORD gameMod = (DWORD)GetModuleHandle("client.dll");

	while (true) {
		if (triggerbot) {
			if (LocalPlayer == NULL) {
				DWORD LocalPlayer = *(DWORD*)(gameMod + dwLocalPlayer);
			}

			//check om noget på crosshari
			int crosshair = *(int*)(LocalPlayer + m_iCrosshairId);
			int localteam = *(int*)(LocalPlayer + m_iTeamNum);

			//der er en fjende i crosshair og om det er en valid spiller
			if (crosshair != 0 && crosshair < 64) {
				uintptr_t entity = *(uintptr_t*)(gameMod + dwEntityList + (crosshair - 1) * 0x10);
				int eTeam = *(int*)(entity + m_iTeamNum);
				int eHealth = *(int*)(entity + m_iHealth);

				if (eTeam != localteam && eHealth > 0 && eHealth < 101) {
					if (triggerbotRandom) {
						
						Sleep((rand() * 250) + 50);
						//sshoot the gun
						*(int*)(gameMod + dwForceAttack) = 5;
						Sleep(20);
						//stop skyde
						*(int*)(gameMod + dwForceAttack) = 4;
					}
					else {
						if (triggerCustomDelay) {
							Sleep(triggerCustomDelay);
							*(int*)(gameMod + dwForceAttack) = 5;
							Sleep(20);
							*(int*)(gameMod + dwForceAttack) = 4;
						}
						else {
							*(int*)(gameMod + dwForceAttack) = 5;
							Sleep(20);
							*(int*)(gameMod + dwForceAttack) = 4;
						}
					}
				}
			}
		
		}
		Sleep(2);
	}
}

DWORD WINAPI BhopThread(LPVOID lp) {
	DWORD gameMod = (DWORD)GetModuleHandle("client.dll");
	DWORD localPlayer = *(DWORD*)(gameMod + dwLocalPlayer);

	while (localPlayer == NULL) {
		DWORD localPlayer = *(DWORD*)(gameMod + dwLocalPlayer);
	}

	while (true) {
		if (bhop) {
			//Detect om vi er på jorden
			DWORD flag = *(BYTE*)(localPlayer + m_fFlags);

			//hvis vi holder space + vi er på jorden (om den er 1 eller 0) - så force jump;
			if (GetAsyncKeyState(VK_SPACE) && flag & (1 << 0)) {
				*(DWORD*)(gameMod + dwForceJump) = 6;
			}
		}
	}
}

DWORD WINAPI GlowThread(LPVOID lp) {
	uintptr_t GlowObj = *(uintptr_t*)(gameMod + dwGlowObjectManager);

	while (true) {
		if (LocalPlayer == NULL) {
			DWORD LocalPlayer = *(uintptr_t*)(gameMod + dwLocalPlayer);
		}

		if (glow) {
			int Localteam = *(int*)(LocalPlayer + m_iTeamNum);
			for (int i = 0; i < 64; i++) {
				//Looping though enteties
				//0x10 is how far each entity is in memory
				uintptr_t Entity = *(uintptr_t*)(gameMod + dwEntityList + i * 0x10);
				if (Entity != NULL) {
					int EntityTeam = *(int*)(Entity + m_iTeamNum);
					int GlowIndex = *(int*)(Entity + m_iGlowIndex);

					if (Localteam != EntityTeam) {
						*(float*)(GlowObj + ((GlowIndex * 0x38) + 0x8)) = EnemyColor.Value.x;
						*(float*)(GlowObj + ((GlowIndex * 0x38) + 0x8)) = EnemyColor.Value.y;
						*(float*)(GlowObj + ((GlowIndex * 0x38) + 0xC)) = EnemyColor.Value.z;
						*(float*)(GlowObj + ((GlowIndex * 0x38) + 0x10)) = EnemyColor.Value.w;
					}
					/*else {
						*(float*)(GlowObj + ((GlowIndex * 0x38) + 0x8)) = TeamColor.Value.x;
						*(float*)(GlowObj + ((GlowIndex * 0x38) + 0x8)) = TeamColor.Value.y;
						*(float*)(GlowObj + ((GlowIndex * 0x38) + 0xC)) = TeamColor.Value.z;
						*(float*)(GlowObj + ((GlowIndex * 0x38) + 0x10)) = TeamColor.Value.w;
					}*/
					*(bool*)(GlowObj + ((GlowIndex * 0x38) + 0x24)) = true;
					*(bool*)(GlowObj + ((GlowIndex * 0x38) + 0x25)) = false;
				}
			}
		}
	}
}

BOOL WINAPI DllMain(HMODULE hMod, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hMod);
		CreateThread(nullptr, 0, MainThread, hMod, 0, nullptr);
		CreateThread(nullptr, 0, ThirdPersonThread, hMod, 0, nullptr);
		CreateThread(nullptr, 0, BhopThread, hMod, 0, nullptr);
		CreateThread(nullptr, 0, TriggerThread, hMod, 0, nullptr);
		CreateThread(nullptr, 0, GlowThread, hMod, 0, nullptr);
		break;
	case DLL_PROCESS_DETACH:
		kiero::shutdown();
		break;
	}
	return TRUE;
}
