// totally legit proggy
//
#include "all_headers.h"
#include "INIReader.h"
#define WS_EX_LAYERED 0x00080000
#ifdef _UNICODE
#define UNICODE
typedef wchar_t TCHAR;
#else
typedef char TCHAR;
#endif // _UNICODE

typedef const TCHAR* LPCTSTR;
/*
using namespace hazedumper::netvars;
using namespace hazedumper::signatures;
*/

#define PLRSZ 0x10
#define SERVERDLL L"server.dll"
#define CLIENTDLL L"client_panorama.dll"
#define ENGINEDLL L"engine.dll"

struct Vector2{
	float x = 0; float y = 0;
};struct Vector3{
	float x = 0; float y = 0; float z = 0;
};
 

DWORD server_dll_base;
DWORD client_dll_base;
DWORD engine_dll_base;

HANDLE hProcess;
HWND csgo_hWnd;
RECT csgo_rect;
HWND overlay_hWnd;
RECT overlay_rect;


HDC hDC;
void get_process_handle();

LPDIRECT3D9EX d3d = NULL;
LPDIRECT3DDEVICE9EX d3ddev = NULL; 
LPD3DXFONT pFont = NULL;
CDraw Draw;
 

struct view_matrix_t{
	float* operator[ ](int index){
		return matrix[index];
	}
	float matrix[4][4];
};
view_matrix_t view_matrix;

Vector3 world_to_screen(Vector3 from);
DWORD game_localplayer_base;
float game_localplayer_coords[3]; 
int game_localplayer_team;
int game_localplayer_index;
int game_max_player_count = 32;

int aim_target_index = -1;
boolean aim_hack_active = true;
boolean aim_weapon_icon_active = true;

void read_my_coords();
void get_view_matrix();  

int read_bytes(LPCVOID addr, int num, LPVOID buf);
void esp();
void render();
//void aim(std::stack<enemy*> copy);
void lock_target(float head_x, float head_y);
void DrawScene(LPDIRECT3DDEVICE9 pDevice);

DWORD dwEntityList;
DWORD dwLocalPlayer;
DWORD dwViewMatrix;
DWORD m_iTeamNum;
DWORD m_iHealth;
DWORD m_bDormant;
DWORD m_vecOrigin;
DWORD m_bSpottedByMask;
DWORD m_dwBoneMatrix;
DWORD m_hActiveWeapon;
DWORD m_iItemDefinitionIndex;

class Player {
public:
	DWORD entity = NULL;
	int index = NULL;
	Vector2 top;
	Vector3 head;
	Vector3 origin;
	Vector3 bottom;
	int team = 0;
	int hp = 0;
	float distance = 0;
	float distanceCross = 0;
	boolean dormant;
	boolean spotted;
	boolean active;
	boolean isLocalPlayer = false;
	int weaponId = 0;
	void loadData(){
		try{
			team = 0;
			read_bytes((LPCVOID*)(entity + m_iTeamNum), 4, &team);
			read_bytes((LPCVOID*)(entity + m_iHealth), 4, &hp);
			read_bytes((LPCVOID*)(entity + m_bDormant), 4, &dormant);
			read_bytes((LPCVOID*)(entity + m_vecOrigin), 12, &origin);
			bottom = world_to_screen(origin);


			isLocalPlayer = entity == game_localplayer_base;

			if(team < 2 || hp < 1 || dormant || bottom.z < 0.01f || (game_localplayer_team == team && !isLocalPlayer) || isLocalPlayer){
				active = false;
				return;
			}


			active = true;
			int spotted_by;
			read_bytes((LPCVOID*)(entity + m_bSpottedByMask), 4, &spotted_by);
			spotted = spotted_by & (1 << game_localplayer_index - 1);
			head = getBonePos(8);

			distance = sqrt(pow(origin.x - game_localplayer_coords[0], 2) + pow(origin.y - game_localplayer_coords[1], 2) + pow(origin.z - game_localplayer_coords[2], 2) * 1.0);

			distanceCross = sqrt(pow((head.y + 10 - (csgo_rect.bottom - csgo_rect.top) / 2), 2) + pow((head.x - (csgo_rect.right - csgo_rect.left) / 2), 2));



			int wi1;
			ReadProcessMemory(hProcess, (LPCVOID*)(entity + m_hActiveWeapon), &wi1, 4, 0);
			wi1 &= 0xFFFF;
			int wi2;
			ReadProcessMemory(hProcess, (LPCVOID*)(client_dll_base + dwEntityList + (wi1 - 1) * 0x10), &wi2, 4, 0);
			ReadProcessMemory(hProcess, (LPCVOID*)(wi2 + 12202), &weaponId, 4, 0);
		} catch(...){
			cout << "Default Exception\n";
		}
	}
	Vector3 getBonePos(int boneId){
		Vector3 input = (getBoneCoord(boneId));
		Vector3 result = world_to_screen(input);
	
		return result;
	}
	Vector3 getBoneCoord(int boneId){
		float vecBone[3]; 
		DWORD plr_bone_matrix; 
		read_bytes((LPCVOID*)(entity + m_dwBoneMatrix), 4, &plr_bone_matrix);
		read_bytes((LPCVOID*)(plr_bone_matrix + 0x30 * boneId + 0x0C), 4, &vecBone[0]);
		read_bytes((LPCVOID*)(plr_bone_matrix + 0x30 * boneId + 0x1C), 4, &vecBone[1]);
		read_bytes((LPCVOID*)(plr_bone_matrix + 0x30 * boneId + 0x2C), 4, &vecBone[2]);
		Vector3 coord = { vecBone[0],vecBone[1],vecBone[2] };
		return coord;
	}
};
Player* players[65];

std::string convLPCWSTRtoString(LPCWSTR wString){
	std::wstring ws(wString);
	return std::string(ws.begin(), ws.end());
}
 
DWORD getOffetFromConfig(std::string section, std::string key){
	try{
		INIReader reader("csgo.toml");
		int value = reader.GetInteger(section, key, -1);
		if(value > -1){
			std::cout << termcolor::cyan << "Loaded: " << section << " - " << key << termcolor::reset << std::endl;
		} else{
			std::cout << termcolor::red << "! Error: " << section << " - " << key << termcolor::reset << std::endl;
		}
		return (DWORD)value;
	} catch(...){
		cout << "Default Exception\n";
	}
} 

int main(int argc, char** argv)
{
	/*
	FILE *sortie;
	char fichier[256];
	std::wstring fichier(MAX_PATH, L'\0');//   <--- HERE s my char table
	const DWORD len = GetCurrentDirectory(fichier.size(), &fichier[0]);
	if(len == 0 || len >= fichier.size()){
		throw std::runtime_error("GetCurrentDirectory failed.");
	}
	fichier.resize(len);
	fichier += L"/fichierlog.txt";
	*/

	std::cout << "Starting " << std::endl << termcolor::green << "[Amiral Router] - CSGO Lite ESP - External" << std::endl << std::endl;
	std::cout << termcolor::red << "If ESP not working:" << std::endl;
	std::cout << termcolor::yellow << "\t1. start csgo.exe with -inscure parameter" << std::endl;
	std::cout << termcolor::yellow << "\t2. start a game with bots" << std::endl;
	std::cout << termcolor::yellow << "\t3. start hazedumper.exe as administrator" << std::endl;
	std::cout << termcolor::yellow << "\t4. restart this app" << std::endl;
	std::cout << termcolor::reset << std::endl;
	std::cout << termcolor::green << "Reading offsets from INI file" << std::endl;
	 

	dwEntityList = getOffetFromConfig("signatures", "dwEntityList");
	dwLocalPlayer = getOffetFromConfig("signatures", "dwLocalPlayer");
	dwViewMatrix = getOffetFromConfig("signatures", "dwViewMatrix");
	m_bDormant = getOffetFromConfig("signatures", "m_bDormant");
	m_iTeamNum = getOffetFromConfig("netvars", "m_iTeamNum");
	m_iHealth = getOffetFromConfig("netvars", "m_iHealth");
	m_vecOrigin = getOffetFromConfig("netvars", "m_vecOrigin");
	m_bSpottedByMask = getOffetFromConfig("netvars", "m_bSpottedByMask");
	m_dwBoneMatrix = getOffetFromConfig("netvars", "m_dwBoneMatrix");
	m_hActiveWeapon = getOffetFromConfig("netvars", "m_hActiveWeapon");
	m_iItemDefinitionIndex = getOffetFromConfig("netvars", "m_iItemDefinitionIndex");
 

	get_process_handle();
	GetWindowRect(csgo_hWnd, &csgo_rect);
	WinMain(0, 0, 0, 1);
	CloseHandle(hProcess);

	return 0;
}

void loadLocalPlayerData(){
	try{
		read_bytes((void*)(client_dll_base + dwLocalPlayer), 4, &game_localplayer_base);

		read_bytes((void*)(game_localplayer_base + m_vecOrigin), 12, &game_localplayer_coords);

		read_bytes((LPCVOID*)(game_localplayer_base + m_iTeamNum), 4, &game_localplayer_team);

		read_bytes((LPCVOID*)(game_localplayer_base + 0x00000064), 4, &game_localplayer_index);

		read_bytes((LPCVOID*)(client_dll_base + dwViewMatrix), 64, &view_matrix);
	} catch(...){
		cout << "Default Exception\n";
	}
}


void esp(){
	try{
		int players_on_map, i, hp, team, spotted_by, spotted, dormant;
		float coords[3];


		loadLocalPlayerData();

		read_bytes((LPCVOID*)(client_dll_base + dwViewMatrix), 64, &view_matrix);


		DWORD entityBase = client_dll_base + dwEntityList;


		for(i = 0; i < game_max_player_count; i++){
			read_bytes((LPCVOID*)(entityBase + i * PLRSZ), 4, &players[i]->entity);
			players[i]->loadData();

		}
	} catch(...){
		cout << "Default Exception\n";
	}
	
}


  
Vector3 world_to_screen(Vector3 pos){ 
	try{
		Vector3 result;
		float _x = view_matrix[0][0] * pos.x + view_matrix[0][1] * pos.y + view_matrix[0][2] * pos.z + view_matrix[0][3];
		float _y = view_matrix[1][0] * pos.x + view_matrix[1][1] * pos.y + view_matrix[1][2] * pos.z + view_matrix[1][3];
		float w = view_matrix[3][0] * pos.x + view_matrix[3][1] * pos.y + view_matrix[3][2] * pos.z + view_matrix[3][3];

		float inv_w = 1.f / w;
		_x *= inv_w;
		_y *= inv_w;

		int width = (int)(csgo_rect.right - csgo_rect.left);
		int height = (int)(csgo_rect.bottom - csgo_rect.top);

		result.x = width * .5f;
		result.y = height * .5f;

		result.x += 0.5f * _x * width + 0.5f;
		result.y -= 0.5f * _y * height + 0.5f;
		result.z = w;

		return result; 
	} catch(const std::exception&){

	}
}


void get_process_handle()
{
	try{
		LPCWSTR WINDOWNAME = L"Counter-Strike: Global Offensive";
		DWORD pid = 0;
		csgo_hWnd = FindWindow(0, WINDOWNAME);
		if(csgo_hWnd == NULL){
			printf("FindWindow failed, %08X\n", GetLastError());
			return;
		}
		GetWindowThreadProcessId(csgo_hWnd, &pid);
		hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, 0, pid);
		if(hProcess == 0){
			printf("OpenProcess failed, %08X\n", GetLastError());
			return;
		}
		hDC = GetDC(csgo_hWnd);
		HMODULE hMods[1024];
		int i;
		if(EnumProcessModules(hProcess, hMods, sizeof(hMods), &pid) == 0){
			printf("enumprocessmodules failed, %08X\n", GetLastError());
		} else{
			for(i = 0; i < (pid / sizeof(HMODULE)); i++){
				TCHAR szModName[MAX_PATH];
				if(GetModuleFileNameEx(hProcess, hMods[i], szModName,
					sizeof(szModName) / sizeof(TCHAR))){
					if(wcsstr(szModName, SERVERDLL) != 0){
						printf("server.dll base: %08X\n", hMods[i]);
						server_dll_base = (DWORD)hMods[i];
					}
					if(wcsstr(szModName, CLIENTDLL) != 0){
						printf("client_panorama.dll base: %08X\n", hMods[i]);
						client_dll_base = (DWORD)hMods[i];
					}
					if(wcsstr(szModName, ENGINEDLL) != 0){
						printf("engine.dll base: %08X\n", hMods[i]);
						engine_dll_base = (DWORD)hMods[i];
					}
					//std::wcout << szModName << std::endl;
				}
			}
		}
	} catch(const std::exception&){

	}
}

int read_bytes(LPCVOID addr, int num, LPVOID buf)
{
	SIZE_T sz = 0;
	int r = ReadProcessMemory(hProcess, addr, buf, num, &sz);
	if (r == 0 || sz == 0) {
		//printf("RPM error, %08X\n", GetLastError());
		return 0;
	}
	return 1;
}



void initD3D(HWND hWnd)
{

	Direct3DCreate9Ex(D3D_SDK_VERSION, &d3d);

	// Create the D3DDevice
	D3DPRESENT_PARAMETERS g_d3dpp;
	ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
	g_d3dpp.Windowed = TRUE;
	g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	g_d3dpp.MultiSampleQuality = D3DMULTISAMPLE_NONE;
	g_d3dpp.BackBufferFormat = D3DFMT_A8R8G8B8;
	g_d3dpp.EnableAutoDepthStencil = TRUE;
	g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
	g_d3dpp.BackBufferCount = 1;
	g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
	g_d3dpp.BackBufferWidth = csgo_rect.right - csgo_rect.left;
	g_d3dpp.BackBufferHeight = csgo_rect.bottom - csgo_rect.top;

	d3d->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, 0, &d3ddev);
	d3ddev->SetRenderState(D3DRS_ALPHABLENDENABLE, true);
	d3ddev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVDESTCOLOR);
	d3ddev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_INVSRCALPHA);
	d3ddev->SetTexture(0, NULL);
	d3ddev->SetPixelShader(0);
	d3ddev->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
	d3ddev->SetRenderState(D3DRS_ALPHABLENDENABLE, true);
	d3ddev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	d3ddev->SetRenderState(D3DRS_ZENABLE, FALSE);
	d3ddev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	d3ddev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTSS_COLORARG1);
	d3ddev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);

	d3ddev->SetRenderState(D3DRS_ZENABLE, true);
	d3ddev->SetRenderState(D3DRS_ALPHABLENDENABLE, true);
	d3ddev->SetRenderState(D3DRS_SCISSORTESTENABLE, true);
	d3ddev->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
	d3ddev->SetRenderState(D3DRS_SRCBLENDALPHA, D3DBLEND_ZERO);
	d3ddev->SetRenderState(D3DRS_DESTBLENDALPHA, D3DBLEND_ONE);

	d3ddev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	d3ddev->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
	d3ddev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	d3ddev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

 
	Draw = CDraw();

	D3DXCreateTextureFromFile(d3ddev, L"C:/Users/Amiral/Downloads/CSGO-Cheat-master/CSGO-Cheat-master/Debug/weapons/42-knife.png", &Draw.weaponTex[42]);
	D3DXCreateSprite(d3ddev, &Draw.weaponSprite[42]);

	//Draw.AddFont(L"Arial", 15, false, false);
	//Draw.AddFont(L"Verdana", 15, true, false);
	//Draw.AddFont(L"Verdana", 13, true, false);
	//Draw.AddFont(L"Comic Sans MS", 30, true, false);

	D3DXCreateFont(d3ddev, 12, 0, FW_BOLD, 1, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial", &pFont);
}

void DrawString(int x, int y, DWORD color, LPD3DXFONT g_pFont, const WCHAR *fmt){
	try{
		RECT FontPos = { x, y, x + 120, y + 16 };
		WCHAR buf[1024] = { L'\0' };
		va_list va_alist;

		va_start(va_alist, fmt);
		vswprintf_s(buf, fmt, va_alist);
		va_end(va_alist);
		g_pFont->DrawText(NULL, buf, -1, &FontPos, DT_NOCLIP, color);
	} catch(...){
		cout << "Default Exception\n";
	}
}

void render(){ 
	try{
		d3ddev->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_RGBA(0, 0, 0, 0), 1.0f, 0); 
		d3ddev->BeginScene();

		DrawScene(d3ddev);

		d3ddev->EndScene();
		d3ddev->Present(NULL, NULL, NULL, NULL);
	} catch(...){
		cout << "Default Exception\n";
	}
}


void DrawScene(LPDIRECT3DDEVICE9 pDevice){
	try{
		Draw.GetDevice(pDevice);
		Draw.Reset();
		if(Draw.Font()) Draw.OnLostDevice();

		for(int i = 0; i < game_max_player_count; i++){
			if(players[i]->active && !players[i]->isLocalPlayer){

				//Draw.Box(players[i]->head.x - 5, players[i]->head.y - 5, 10, 10, 1, RED(255));

				D3DCOLOR playerColor = aim_target_index == i && aim_hack_active ? D3DCOLOR_ARGB(200, 0, 255, 0) : D3DCOLOR_ARGB(200, 255, 0, 0);
				Draw.CircleFilled(players[i]->head.x, players[i]->head.y, 3, 0, full, 10, playerColor);
				Draw.Line(csgo_rect.left + (csgo_rect.right - csgo_rect.left) / 2, csgo_rect.top, players[i]->head.x, players[i]->head.y - 5, 1.0f, true, playerColor);


				wchar_t m_reportFileName[128];
				//swprintf_s(m_reportFileName, L"%f - %f - %f", players[i]->origin.x, players[i]->origin.y, players[i]->origin.z);
				swprintf_s(m_reportFileName, L"%d", players[i]->hp);
				DrawString(players[i]->head.x + 10, players[i]->head.y - 5, playerColor, pFont, m_reportFileName);

				if(aim_weapon_icon_active){
					Draw.WeaponLogo(players[i]->head.x - 15, players[i]->head.y - 24, players[i]->weaponId, d3ddev);
				}
			}
		}
	} catch(...){
		cout << "Default Exception\n";
	}

	try{
		if(GetAsyncKeyState(VK_NUMPAD1)){
			aim_hack_active = !aim_hack_active;
			Sleep(300);
		}
		if(aim_hack_active){
			DrawString(20, 20, D3DCOLOR_ARGB(255, 0, 255, 0), pFont, L"NUM1 - AIM HACK : ON");
		} else{
			DrawString(20, 20, D3DCOLOR_ARGB(255, 255, 0, 0), pFont, L"NUM1 - AIM HACK : OFF");
		}
		if(GetAsyncKeyState(VK_NUMPAD2)){
			aim_weapon_icon_active = !aim_weapon_icon_active;
			Sleep(300);
		}
		if(aim_weapon_icon_active){
			DrawString(20, 35, D3DCOLOR_ARGB(255, 0, 255, 0), pFont, L"NUM2 - WEAPONS ICONS : ON");
		} else{
			DrawString(20, 35, D3DCOLOR_ARGB(255, 255, 0, 0), pFont, L"NUM2 - WEAPONS ICONS : OFF");
		}
	} catch(...){
		cout << "Default Exception\n";
	}
	
	return;
	
}


float DistanceBetweenCross(float X, float Y){
	float ydist = (Y - (csgo_rect.bottom - csgo_rect.top)/2);
	float xdist = (X - (csgo_rect.right - csgo_rect.left)/2);
	float Hypotenuse = sqrt(pow(ydist, 2) + pow(xdist, 2));
	return Hypotenuse;
}

bool new_data = false;

void findAimTarget(){
	float targetDistance = 9999999.0;
	aim_target_index = -1;
	for(int i = 0; i < game_max_player_count; i++){
		if(players[i]->active){
			if(players[i]->distanceCross < 50 && players[i]->distanceCross > 1){
				if(players[i]->distance < targetDistance){
					aim_target_index = i;  
					targetDistance = players[i]->distance;
				}
			}
		} 
	}
}
void aim() {
	new_data = true;
	findAimTarget();
	if (aim_hack_active && GetAsyncKeyState(1)) {
		if(aim_target_index != -1){
			std::async(lock_target, players[aim_target_index]->head.x - (csgo_rect.right - csgo_rect.left) / 2, players[aim_target_index]->head.y - (csgo_rect.bottom - csgo_rect.top) / 2);
		} 
	} 
}

void lock_target(float head_x, float head_y){
	new_data = false;

	float dx = head_x;
	float dy = head_y;

	INPUT input = { 0 };
	 
	input.mi.dx = dx * (dx > -10 && dx < 10 ? 0.5f : 1.2f);
	input.mi.dy = dy * (  dy < 10 ? 0.5f : 1.2f);
	input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK | MOUSEEVENTF_MOVE;
	input.type = INPUT_MOUSE;

	SendInput(1, &input, sizeof INPUT);



	Sleep(1);
	if(new_data) return;
}
 
 


/*** OVERLAY WINDOW STUFF ***/
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	LPCWSTR lpClassName = L"AmiralRouterCSGoLiteEspClass";
	LPCWSTR lpWindowName = L"Amiral Router - CSGO Lite ESP";

	if(IsWindow(csgo_hWnd)){
		GetWindowRect(csgo_hWnd, &csgo_rect);
		MapWindowPoints(HWND_DESKTOP, GetParent(csgo_hWnd), (LPPOINT)&csgo_rect, 2);
	}

	HINSTANCE hInstC = GetModuleHandle(0);
	MSG msg;
	long Loops = 0;
	WNDCLASSEX window_class;
	window_class.cbClsExtra = NULL;
	window_class.cbSize = sizeof(WNDCLASSEX);
	window_class.cbWndExtra = NULL;
	window_class.hbrBackground = (HBRUSH)CreateSolidBrush(D3DCOLOR_ARGB(0, 0, 0, 0));
	window_class.hCursor = LoadCursor(0, IDC_ARROW);
	window_class.hIcon = LoadIcon(0, IDI_APPLICATION);
	window_class.hIconSm = LoadIcon(0, IDI_APPLICATION);
	window_class.hInstance = hInstC;
	window_class.lpfnWndProc = WindowProc;
	window_class.lpszClassName = lpWindowName;
	window_class.lpszMenuName = lpWindowName;
	window_class.style = CS_VREDRAW | CS_HREDRAW;
	RegisterClassEx(&window_class);
	overlay_hWnd = CreateWindowEx(
		WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED,
		lpWindowName, lpWindowName,
		WS_POPUP,
		csgo_rect.left, csgo_rect.top, csgo_rect.right - csgo_rect.left, csgo_rect.bottom - csgo_rect.top,
		0, 0, 0, 0
	);
	//window_hWnd = CreateWindowEx(WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED, "D3 testing", "D3 testing",WS_BORDER, 1, 1, 800, 600, 0, 0, 0, 0);
	SetLayeredWindowAttributes(overlay_hWnd, 0, 255, LWA_ALPHA);
	MARGINS margin = { -1 };
	DwmExtendFrameIntoClientArea(overlay_hWnd, &margin);

	SetWindowLong(overlay_hWnd, GWL_EXSTYLE, GetWindowLong(overlay_hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);

	ShowWindow(overlay_hWnd, SW_NORMAL);
	UpdateWindow(overlay_hWnd);
	initD3D(overlay_hWnd);


	for(int i = 0; i < 64; i++){
		players[i] = new Player();
	}

	while (TRUE)
	{
		if (!FindWindow(NULL, lpWindowName)) ExitProcess(0);
		
		esp(); 
		render();
		aim();
		/**/
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (msg.message == WM_QUIT)
			exit(0);

	}
	return msg.wParam;;
}

BOOL OnCreate(HWND hwnd, LPCREATESTRUCT lpCreateStruct)
{
	return TRUE;
}

void OnDestroy(HWND hwnd)
{
	PostQuitMessage(0);
}

void OnQuit(HWND hwnd, int exitCode)
{

}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam){ true;

	switch(msg){
	case WM_SIZE: 
		return 0;
	case WM_SYSCOMMAND:
		if((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
		break;
	case WM_DESTROY:
		::PostQuitMessage(0);
		return 0;
	}
	return ::DefWindowProc(hWnd, msg, wParam, lParam);
}