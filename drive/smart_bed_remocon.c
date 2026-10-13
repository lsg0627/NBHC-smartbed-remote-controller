#include "../app/main.h"
#include "smart_bed_remocon.h"
#include "etc_driver/misc.h"


U8 conform_key_run = 0;
int running_massage_type = -1;	// 현재 동작 중인 마사지 (-1: 없음, 0~11: 마사지 번호)
// 마사지 화면 스크롤 애니메이션 — UP/DOWN 누르면 카드 슬라이드
int massage_anim_offset = 0;	// (구) 슬라이드 오프셋 — 미사용
int massage_anim_frame  = 0;	// 마사지 측면매트리스 미리보기 애니메이션 프레임(틱마다 +1)
int massage_wave_frame  = 0;	// 파도타기 전용 웨이브 프레임(~20ms마다 +1, 부드러운 보간용)
// 마사지 설정(조합에 따라 동작시간 결정 예정)
int massage_intensity = 0;		// 강도  0:약 1:중 2:강 (기본 약)
int massage_speed     = 0;		// 스피드 0:느리게 1:보통 2:빠르게 (기본 느리게)
int massage_repeat    = 0;		// 반복  0:1회 1:2회 2:무한 (기본 1회)
// 마사지 화면 2단 흐름: 선택(0) → 확인 → 설정(1). config_anim=전환 진행도(0~100, tick이 갱신)
int massage_config_mode  = 0;	// 0:선택 모드 1:설정 모드
int massage_config_anim  = 0;	// 전환 진행도 0(선택)~100(설정)
int massage_setting_focus = 0;	// 설정 포커스 0:강도 1:스피드 2:반복
int ulcer_config_mode = 0;		// 욕창 사용자맞춤 설정창(0:리스트 1:설정) — 마사지 config와 동일 방식
int ulcer_config_anim = 0;		// 0(리스트)~100(설정) 전환 진행도
int ulcer_intensity = 0;		// 높이 0:약 1:중 2:강
int ulcer_speed     = 0;		// 속도 0:느림 1:보통 2:빠름
int ulcer_setting_focus = 0;	// 행 포커스 0:높이 1:속도
BED_STATUS_DATA bed_status = {0};  // 침대 상태 (ESP32에서 2초 주기 수신)
U8 g_presence_detect = 0;          // 재실 감지 자동종료 설정 echo (BedStatus [16] 바이트, 구조체 밖 별도 파싱)
U8 g_bed_variant = 1;              // 베드 종류 (0=AnyBed, 1=NineBed). BedStatus [17]. 기본 NineBed(높이 탭 표시)
U8 body_set_max[BODY_LEVIT_MAX] = {0,};
U8 bar[16] = {0,};
bool body_info = false;
U8 pressure_map[PRESSURE_ROWS][PRESSURE_COLS] = {{0,}};
S16 motor_positions[11] = {0,};	// ESP32에서 받은 11 모터 위치 (id 0~10, raw pulses)

// 마사지 UI 인덱스 (0~6) → CMD offset
// ESP32에서 집중/거꾸리/무중력/호흡 삭제 + 재번호매김(0x30~0x37) 완료 → offset이 identity.
// UI 7개 = CMD offset 0~6 (파도타기, 지압, 추나, 스트레칭, 롤링, 시소, 수면).
// 트위스트(offset 7 = 0x37)는 CMD에는 존재하지만 UI 미노출.
static const U8 massage_cmd_offset[MASSAGE_UI_MAX] = {
	0, 1, 2, 3,          // 파도타기, 지압, 추나, 스트레칭
	4, 5, 6              // 롤링, 시소, 수면
};
BODY levitate[LEVIT_MAX];// 교대 부양(일반/집중/수면)
BODY dispersion;// 체압분산
BODY massage[12];	// 마사지
BODY patient_care[3];	// 환자 돌봄케어
BODY heat;	// 온열
BODY volume;	// 음량
BODY temp_body;

CURSOR cursor;



U16 radio_xy[4][2] = {
	{ RADIO_X, RADIO_HEAD_Y},// 머리 
	{ RADIO_X, RADIO_BODYUP_Y},	// 몸통(상부)
	{ RADIO_X, RADIO_BODYDWN_Y},	// 몸통(하부)
	{ RADIO_X, RADIO_LEG},// 다리
};

U16 num2_xy[2][2] = {
// 2num
	{ LABLE0_X, (LABLE0_Y + 50)},
	{ LABLE1_X, (LABLE1_Y + 70)},
};
U16 num3_xy[3][2] = {
// 3num
	{ LABLE0_X, LABLE0_Y},
	{ LABLE1_X, LABLE1_Y},
	{ LABLE2_X, LABLE2_Y},
};
//////////////////////////////////////////////////////////////////////////////
// UTF-8 → EUC-KR 변환 유틸리티
//////////////////////////////////////////////////////////////////////////////
extern int unicode_to_oem(unsigned short src);

// UTF-8 문자열을 EUC-KR로 변환
static void utf8_to_euckr(const char* utf8, char* euckr, int max_len)
{
	int i = 0, j = 0;
	while(utf8[i] && j < max_len - 2){
		U8 c = (U8)utf8[i];
		if(c < 0x80){
			// ASCII
			euckr[j++] = c;
			i++;
		}
		else if((c & 0xE0) == 0xC0){
			// 2바이트 UTF-8
			U16 uni = ((c & 0x1F) << 6) | (utf8[i+1] & 0x3F);
			int oem = unicode_to_oem(uni);
			if(oem > 0xFF){ euckr[j++] = (oem >> 8); euckr[j++] = (oem & 0xFF); }
			else { euckr[j++] = (oem & 0xFF); }
			i += 2;
		}
		else if((c & 0xF0) == 0xE0){
			// 3바이트 UTF-8 (한글 범위)
			U16 uni = ((c & 0x0F) << 12) | ((utf8[i+1] & 0x3F) << 6) | (utf8[i+2] & 0x3F);
			int oem = unicode_to_oem(uni);
			if(oem > 0xFF){ euckr[j++] = (oem >> 8); euckr[j++] = (oem & 0xFF); }
			else { euckr[j++] = (oem & 0xFF); }
			i += 3;
		}
		else{
			i++;
		}
	}
	euckr[j] = '\0';
}

// 한글 텍스트 출력 래퍼
static void draw_text_kr(EGL_FONT* font, int x, int y, const char* utf8_str)
{
#if USE_PRETENDARD_FONT
	// Pretendard bmpfont는 UTF-8 native — 변환 없이 그대로 넘김
	bmpfont_draw(font, x, y, utf8_str);
#else
	// SDK bitfont는 EUC-KR expects → UTF-8을 변환 후 bitfont_draw 호출
	char euckr[256];
	utf8_to_euckr(utf8_str, euckr, sizeof(euckr));
	bitfont_draw(font, x, y, euckr);
#endif
}

// 16px 한글 폰트 텍스트 폭 계산 — 탭 라벨 가운데 정렬용
// - Pretendard bmpfont: EGL API로 실제 폰트 metric
// - SDK bitfont: UTF-8 못 다루므로 기존 고정폭 계산 유지 (한글=16, ASCII=8)
static int kr_text_width_16(const char* utf8_str)
{
	if(!utf8_str) return 0;
#if USE_PRETENDARD_FONT
	if(!g_pFontKor16) return 0;
	int w = text_width(g_pFontKor16, utf8_str);
	return (w < 0) ? 0 : w;
#else
	int width = 0, i = 0;
	while(utf8_str[i]){
		U8 c = (U8)utf8_str[i];
		if(c < 0x80)                 { width += 8;  i += 1; }
		else if((c & 0xE0) == 0xC0)  { width += 16; i += 2; }
		else if((c & 0xF0) == 0xE0)  { width += 16; i += 3; }
		else                         { i += 1; }
	}
	return width;
#endif
}

// 종료 화면 그리기 (progress_lcd_display에서 호출)
void shutdown_draw(void)
{
	set_draw_target(getbackframe());
	draw_rectfill(0, 0, 320, 480, MAKE_COLORREF(0, 0, 0));
	egl_font_set_color(g_pFontKor, MAKE_COLORREF(255, 255, 255));
	draw_text_kr(g_pFontKor, 95, 226, "종료합니다.");
	flip();
}

// 종료 화면 표시 (LCD OFF 직전, 딜레이 포함)
void show_shutdown_screen(void)
{
	shutdown_draw();
	delayms(1500);
}

// ---------------------------------------------------------------------------
// 공통 상태 오버레이: 화면 상단(타이틀 아래)에 현재 동작 상태 표시
// dispersion_draw, levitate_draw, massage_draw 등에서 호출
// ---------------------------------------------------------------------------
// 모드 코드 → 기본 이름만 (상태 접미사 제외)
const char* get_mode_name(U8 mode) {
	static char buf[24];
	switch(mode) {
		case 0x00: return "";
		// 카테고리별 nibble 재배정
		// [1] 체압분산
		case 0x10: return "체압분산";
		// [2] 교대부양
		case 0x20: return "교대부양 표준";
		case 0x21: return "교대부양 집중";
		case 0x22: return "교대부양 수면";
		// [3] 마사지 (0x30~0x37)
		case 0x30: return "파도타기";
		case 0x31: return "지압";
		case 0x32: return "추나";
		case 0x33: return "스트레칭";
		case 0x34: return "롤링";
		case 0x35: return "시소";
		case 0x36: return "수면";
		case 0x37: return "트위스트";   // UI 미노출
		// [4] 돌봄
		case 0x40: return "틸팅";       // ReadyTilt
		case 0x46: return "식사모드";
		case 0x47: return "거꾸리";
		case 0x48: return "머리감기";
		case 0x49: return "배변";
		// [6] 무중력/플랫
		case 0x60: return "수평 복귀";
		case 0x61: return "무중력";
		default:
			sprintf(buf, "모드(0x%02X)", mode);
			return buf;
	}
}

// 모드 + 상태 → 풀 텍스트
// stdby_in_progress=true: 사용자 전원 버튼 (로컬)
//   stdby_initial_mode != 0 → "종료중", == 0 → "초기화 중"
// external_stopping=true: 태블릿 등 외부에서 종료 시작 → "종료중"
// 그 외:
//   state 1 + mode!=0 → "[mode] 동작중"
//   state 2 + mode!=0 → "[mode] 일시정지"
//   else → "모드를 선택해 주세요"
const char* get_mode_status_text(U8 mode, U8 state) {
	// 종료(stdby)/외부 종료 진행 중이면 종료 상태 우선 표시.
	if(stdby_in_progress) {
		if(stdby_initial_mode == 0) return "초기화 중";
		return "종료중";
	}
	if(external_stopping) return "종료중";
	// 모드→모드 전환의 전환 홈(이전 모드 종료 + 재홈) 진행 중 → "종료중".
	//   홈 완료(init_home_pending 클리어) 후에야 새 모드명이 뜬다.
	if(init_home_pending && mode_switch_from_running) return "종료중";
	// 모드가 실행 중이면 모드명 — 시작 즉시 표시하고, 시작 홈(state=3) 중에도 계속 유지.
	//   init_home_pending("초기화 중")보다 우선 → 교대부양 시작 즉시 "교대부양"이 뜨고
	//   초기화중/종료중을 거치지 않는다. (동작중/일시정지 구분은 좌측 아이콘으로.)
	if(mode != 0 && (state == 1 || state == 2 || state == 3)) {
		if(mode == 0x20) return "교대부양";			// 표준 (전시 10초/일반 5분 구분 없이 "교대부양")
		if(mode == 0x21) return "교대부양 3분";		// 집중
		if(mode == 0x22) return "교대부양 60분";		// 수면
		return get_mode_name(mode);					// 그 외: 모드명만
	}
	// 모드가 아직 0인데 홈 진행 중(드묾) → 초기화 중.
	if(init_home_pending) return "초기화 중";
	return "모드를 선택해 주세요";
}

// 압력값(0~255) → 색상 변환 (정의는 파일 하단). draw_status_overlay에서 사용.
extern U32 pressure_to_color(U8 value);

// ---------------------------------------------------------------------------
// 아이콘 헬퍼 (PDF 홈화면용 작은 픽토그램)
// ---------------------------------------------------------------------------

// 플레이 + 정지 합친 아이콘 ▶❚ (대기 상태 등 기본)
static void draw_play_icon(int x, int y, int size, U32 color)
{
	int i;
	int tri_w = (size + 1) / 2;
	int bar_w = 3;
	for(i = 0; i < tri_w; i++) {
		int h = size - 2 * i;
		if(h <= 0) break;
		draw_rectfill(x + i, y + i, 1, h, color);
	}
	draw_rectfill(x + size - bar_w, y, bar_w, size, color);
}

// 일시정지 아이콘 ‖ — 총 너비 = size (대기중 ▶❚와 동일)
// 막대 폭 size/3, 갭 size/3 → 2*(size/3) + size/3 = size
static void draw_pause_icon(int x, int y, int size, U32 color)
{
	int bar_w = size / 3; if(bar_w < 2) bar_w = 2;
	int gap = size - 2 * bar_w;
	draw_rectfill(x, y, bar_w, size, color);
	draw_rectfill(x + bar_w + gap, y, bar_w, size, color);
}

// 재생 아이콘 ▶ (일시정지 상태) — 18px 너비 (다른 아이콘과 동일)
static void draw_triangle_icon(int x, int y, int size, U32 color)
{
	int i;
	int steps = (size + 1) / 2;
	for(i = 0; i < steps; i++) {
		int h = size - 2 * i;
		if(h <= 0) break;
		draw_rectfill(x + i * 2, y + i, 2, h, color);	// 2px 폭 × 9단계 = 18 폭
	}
}

// 돌봄 모드 중 무중력/자세제어 차단 안내 오버레이 — 화면 중앙 작은 박스, 2줄 중앙정렬.
//   [중요] 각 화면 draw는 back 프레임에 그린 뒤 flip()으로 표시한다(핑퐁 더블버퍼).
//   이 함수는 그 flip() 이후에 불리므로, 지금 표시 중인 front 프레임에 직접 덮어 그린다.
//   여기서 다시 flip()하면 아직 오버레이가 없는 back(stale) 프레임이 표시되어 메시지가 사라진다.
void draw_care_block_message(void)
{
	set_draw_target(getfrontframe());	// 현재 표시 중인 프레임에 직접 그림 (flip 없이 즉시 반영)

	int bw = 300, bh = 92;
	int bx = (320 - bw) / 2;	// 가로 중앙 (화면폭 320)
	int by = 194;				// 세로 중앙 부근
	draw_roundrectfill(bx, by, bw, bh, 12, MAKE_COLORREF(40, 42, 50));	// 어두운 배경
	draw_roundrect(bx, by, bw, bh, 12, MAKE_COLORREF(255, 90, 90));		// 빨간 테두리

	const char *l1 = "돌봄 모드 실행중입니다";
	const char *l2 = "종료 후 다시 이용해 주시기 바랍니다";
	int w1 = kr_text_width_16(l1);
	int w2 = kr_text_width_16(l2);
	egl_font_set_color(g_pFontKor16, MAKE_COLORREF(255, 255, 255));
	draw_text_kr(g_pFontKor16, bx + (bw - w1) / 2, by + 26, l1);
	draw_text_kr(g_pFontKor16, bx + (bw - w2) / 2, by + 54, l2);
	// flip() 호출하지 않음 — front 프레임에 직접 그렸으므로 이미 화면에 보인다.
}

// ▲ 위 화살표 (꼭짓점 위). cx=중심X, top=상단Y, w=밑변 폭, h=높이
static void draw_arrow_up(int cx, int top, int w, int h, U32 color)
{
	int j;
	for(j = 0; j < h; j++){
		int ww = (w * (j + 1)) / h;		// 위(좁음)→아래(넓음)
		if(ww < 1) ww = 1;
		draw_rectfill(cx - ww / 2, top + j, ww, 1, color);
	}
}
// ▼ 아래 화살표 (꼭짓점 아래)
static void draw_arrow_down(int cx, int top, int w, int h, U32 color)
{
	int j;
	for(j = 0; j < h; j++){
		int ww = (w * (h - j)) / h;		// 위(넓음)→아래(좁음)
		if(ww < 1) ww = 1;
		draw_rectfill(cx - ww / 2, top + j, ww, 1, color);
	}
}

// 온도계 아이콘 (큰 사이즈, 12w × 27h)
static void draw_thermo_icon(int x, int y, U32 color)
{
	draw_roundrectfill(x + 1, y + 17, 10, 10, 5, color);	// 구근 (10×10)
	draw_rectfill(x + 4, y, 4, 19, color);					// 스템 (4×19)
}

// 스피커 아이콘 (큰 사이즈, 14w × 25h, 콘텐츠 y+1~y+25)
static void draw_speaker_icon(int x, int y, int H, U32 color)	// H=나팔 최대 높이(전체 크기)
{
	int bw = H * 5 / 26, bh = H * 10 / 26, cw = H * 10 / 26;
	int cy = y + H / 2;										// 세로 중심
	int i;
	if(bw < 2) bw = 2;
	if(cw < 3) cw = 3;
	draw_rectfill(x, cy - bh / 2, bw, bh, color);			// 드라이버 박스
	for(i = 0; i <= cw; i++) {								// 나팔(콘) — 대칭 확장
		int hh = bh + ((H - bh) * i) / cw;
		draw_rectfill(x + bw + i, cy - hh / 2, 1, hh, color);
	}
}

// 공용 상단 상태 헤더 — 정의는 아래(massage_draw 앞)에 있으나, home_draw/dispersion_draw 등에서
//   먼저 호출하므로 전방 선언.
static void draw_status_header(void);
static void draw_status_content(int bx, int by, int bw, int bh, bool use_scroll);	// 동작 상태(아이콘+텍스트)
static void draw_human_silhouette(void);											// 그리드 위 사람 실루엣(외곽선)
static void draw_time_content_compact(int bx, int by, int bw, int bh);				// 경과시간(컴팩트)
static void draw_status_header_cluster(int bx0, bool frame);						// 온도/음량/무중력 (bx0=좌측시작, frame=테두리)

// 로딩 스피너 — 8 dots 원형, 위상에 따라 밝기 그라데이션으로 회전 표현
// phase: 0~7 — 가장 밝은 dot의 인덱스
void draw_loading_spinner(int cx, int cy, U32 phase)
{
	int r = 25;	// 반지름
	int sz = 7;	// dot 크기
	// 8 위치(시계방향, 12시 시작): radius=25, 대각선 ≈ 25*0.707 ≈ 17
	int dx[8] = {  0, 17, 25, 17,  0,-17,-25,-17};
	int dy[8] = {-25,-17,  0, 17, 25, 17,  0,-17};
	// 밝기 그라데이션: rel=0(현재 활성) → rel=7(가장 오래됨)
	U8 br[8] = { 255, 220, 180, 140, 100,  75,  55,  40 };
	int i;
	for(i = 0; i < 8; i++) {
		int rel = (int)((phase - i + 8) & 7);
		U8 b = br[rel];
		U32 c = MAKE_COLORREF(b, b, b);
		int x = cx + dx[i] - sz / 2;
		int y = cy + dy[i] - sz / 2;
		draw_roundrectfill(x, y, sz, sz, sz / 2, c);
	}
}

// 초기화(호밍) 화면 — 등판/다리판/매트리스 등 어떤 축이 초기화 중이든 공통으로 표시.
// 침대 그림 + 회전 스피너 + 안내 문구. progress_lcd_display에서 모든 화면 위에 오버레이된다.
void homing_draw(void)
{
	set_draw_target(getbackframe());
	draw_surface(home_img, 0, 0);						// home 이미지(침대 그림, boot.suf)
	draw_loading_spinner(160, 240, loading_anim_phase);	// 회전 표시(스피너)
	flip();
}

// 모래시계 아이콘 ⏳ — 위는 윤곽만, 아래는 모래색으로 채움
// sz=18 기준: 상단바(2) + 위 삼각형 윤곽(7) + 핀치(1) + 아래 삼각형 채움(6) + 하단바(2)
static void draw_hourglass_icon(int x, int y, U32 frame_color)
{
	int sz = 18;
	int i;
	U32 SAND = MAKE_COLORREF(255, 200, 100);	// 모래색 (노란빛)
	// 상단/하단 프레임 가로바
	draw_rectfill(x, y, sz, 2, frame_color);
	draw_rectfill(x, y + sz - 2, sz, 2, frame_color);
	// 위 삼각형: 좌/우 사선 윤곽만 (속 비움)
	for(i = 0; i < 7; i++) {
		int w = 16 - 2 * i;	// 16,14,12,10,8,6,4
		int xx = x + (sz - w) / 2;
		draw_rectfill(xx, y + 2 + i, 1, 1, frame_color);			// 좌측 사선
		draw_rectfill(xx + w - 1, y + 2 + i, 1, 1, frame_color);	// 우측 사선
	}
	// 가운데 핀치 (가장 좁은 행, 폭 2)
	draw_rectfill(x + (sz / 2) - 1, y + 9, 2, 1, frame_color);
	// 아래 삼각형: 모래색으로 채움 (모래가 쌓인 모습)
	for(i = 0; i < 6; i++) {
		int w = 4 + 2 * i;	// 4,6,8,10,12,14
		draw_rectfill(x + (sz - w) / 2, y + 10 + i, w, 1, SAND);
	}
}

// ---------------------------------------------------------------------------
// PDF 홈화면 — 4영역 구성 (모든 패널 빨간 보더):
//  [헤더] 모드 상태 알약 + 온열/음량 표시
//  [시간] 모래시계 + 경과/대기 시간
//  [체압] 실시간 체압 분포 + 인체 실루엣 + 하단 16칸 바 스트립
//  [범례] 저/중/고/위험
// ---------------------------------------------------------------------------
void draw_status_overlay(void)
{
	char buf[32];
	U32 CELL_EMPTY = MAKE_COLORREF(150, 152, 158);	// 빈 셀(대기) 색
	U32 BAR_OFF    = MAKE_COLORREF(150, 152, 158);
	U32 BAR_ON     = MAKE_COLORREF(80, 200, 80);

	// 상단 — 경과시간 박스(좌) + 온도/음량/무중력 클러스터(우) + 동작상태 넓은 박스.
	//   (일반/전시 동일 — 일반버전 사진 디자인은 원상복구)
	(void)buf;
	{
		// 온열/사운드/무중력 클러스터 — 좌측 상단으로 이동, 테두리 없음(상태바 스타일).
		draw_status_header_cluster(5, false);
	}
	// 동작 상태 — 위/아래 가로 구분선(전 화면 통일: 55/99, 내용 bh=44)
	draw_rectfill(5, 55, 310, 2, MAKE_COLORREF(90, 95, 105));	// 위 구분선
	draw_rectfill(5, 99, 310, 2, MAKE_COLORREF(90, 95, 105));	// 아래 구분선
	draw_status_content(5, 55, 310, 44, false);	// 경과시간은 이 영역 우측에(모드 표시 중일 때만)

	// ===== 3) 체압 분포 — 중앙 7×11 그리드 + 우측 세운 범례(둘 다 확대) + 하단 측면 매트리스 =====
	{
		const int cols = PRESSURE_COLS;			// 7
		const int rows = PRESSURE_ROWS;			// 11
		const int cw = 24, ch = 28;				// 확대된 셀 (가로24 × 세로28)
		const int px = 26, py = 30;				// 피치
		int gy0 = 105;							// 상단(동작상태 아래)
		int gh  = rows * py - 2;				// 세로 전체 높이(328) → 105~433
		int GW  = cols * px - 2;				// 그리드 폭(180)
		int lw = 18;							// 범례 그라데이션 폭
		int gx0 = 160 - GW / 2;					// 그리드 화면 정중앙(70~250)
		int lx = 260;							// 그라데이션(좌), 라벨(우) — 위치 교체
		int tx = lx + lw + 5;					// 라벨 = 그라데이션 오른쪽(283~)
		int r, c;

		// --- 중앙: 7×11 그리드 (시계90° 회전: row→세로, col→가로) ---
		for(r = 0; r < rows; r++){
			bool is_even = ((r & 1) == 0);
			for(c = 0; c < cols; c++){
				int cell_idx = is_even ? (cols - 1 - c) : c;
				U8 v = pressure_map[r][cell_idx];
				draw_roundrectfill(gx0 + (cols - 1 - c) * px, gy0 + r * py, cw, ch, 3,
					pressure_to_color(v));
			}
		}
		draw_human_silhouette();	// 그리드 위 사람 실루엣(외곽선만) 중앙 오버레이

		// --- 우측: 세운 범례(확대) — 세로 그라데이션(위=고, 아래=저) + 라벨 ---
		{
			int yy;
			int maxv = 255;						// 맨아래 파랑(1) → 맨위 빨강(255)
			for(yy = 0; yy < gh; yy++){
				U8 v = (U8)(maxv - (yy * (maxv - 1)) / (gh - 1));	// 위=빨강, 아래=파랑
				draw_rectfill(lx, gy0 + yy, lw, 1, pressure_to_color(v));
			}
			egl_font_set_color(g_pFontKor16, MAKE_COLORREF(220, 220, 220));
			if(bed_status.reserved){
				draw_text_kr(g_pFontKor16, tx, gy0 + 2,          "위험");
				draw_text_kr(g_pFontKor16, tx, gy0 + gh/3 - 8,   "고");
				draw_text_kr(g_pFontKor16, tx, gy0 + 2*gh/3 - 8, "중");
				draw_text_kr(g_pFontKor16, tx, gy0 + gh - 18,    "저");
			} else {
				draw_text_kr(g_pFontKor16, tx, gy0 + 2,        "고");
				draw_text_kr(g_pFontKor16, tx, gy0 + gh/2 - 8, "중");
				draw_text_kr(g_pFontKor16, tx, gy0 + gh - 18,  "저");
			}
		}

		// --- 그리드와 측면 매트리스 사이 희미한 구분선 (동작상태 구분선과 동일 길이 5~315) ---
		draw_rectfill(5, 440, 310, 1, MAKE_COLORREF(62, 66, 76));

		// --- 하단(가장 아래): 측면 매트리스(가로) — 고정 바 + 11칸(호밍 시 붙고 상승 시 위로) ---
		{
			int scw = 24, sch = 7, sgap = 2, i;		// 셀 가로 24 (그리드 셀과 동일)
			int sw = rows * (scw + sgap) - sgap;	// 11칸 전체 폭(284)
			int hx = 160 - sw / 2;					// 가운데 정렬(18~302)
			int hy = 461, fy = 468;					// 화면 최하단
			draw_roundrectfill(hx, fy, sw, 7, 3, MAKE_COLORREF(55, 60, 70));	// 고정 바 = 11칸 전체 폭
			for(i = 0; i < rows; i++){
				int off = motor_position_to_offset(motor_positions[i]) / 2;		// 0~15
				draw_roundrectfill(hx + i * (scw + sgap), hy - off, scw, sch, 3,
					bar[i] ? BAR_ON : BAR_OFF);
			}
		}
	}
}

// 낙상 경고 화면 그리기: 까만 배경 + 빨간 원 + "낙상 경고" 텍스트
void fall_alert_draw(void)
{
	set_draw_target(getbackframe());

	// 까만 배경
	draw_rectfill(0, 0, 320, 480, MAKE_COLORREF(0, 0, 0));

	// 빨간 원 (중앙, 반지름 80 — roundrectfill로 원형 근사)
	draw_roundrectfill(80, 120, 160, 160, 80, MAKE_COLORREF(220, 30, 30));

	// 원 안에 흰색 느낌표
	egl_font_set_color(g_pFont48, MAKE_COLORREF(255, 255, 255));
	bmpfont_draw(g_pFont48, 148, 160, "!");

	// "낙상 경고" 텍스트 (원 아래)
	egl_font_set_color(g_pFontKor, MAKE_COLORREF(255, 60, 60));
	draw_text_kr(g_pFontKor, 96, 310, "낙상 경고");

	// 안내 메시지
	egl_font_set_color(g_pFontKor16, MAKE_COLORREF(180, 180, 180));
	draw_text_kr(g_pFontKor16, 52, 370, "확인 버튼을 눌러 해제하세요");

	flip();
}

// 부팅 확인창 (spec §6)
// startup_pending=1 → 사용자가 확인 키를 3초간 눌러야 홈이 시작된다.
// 우발적 클릭 방지가 목적이므로 단일 클릭으로 진행시키면 안 된다.
void startup_confirm_draw(void)
{
	int i;
	int filled;

	set_draw_target(getbackframe());

	draw_rectfill(0, 0, 320, 480, MAKE_COLORREF(0, 0, 0));

	// 상단 주황색 경고 띠
	draw_rectfill(0, 60, 320, 6, MAKE_COLORREF(230, 150, 30));

	egl_font_set_color(g_pFontKor, MAKE_COLORREF(255, 200, 60));
	draw_text_kr(g_pFontKor, 44, 110, "침대를 시작합니다");

	egl_font_set_color(g_pFontKor16, MAKE_COLORREF(200, 200, 200));
	draw_text_kr(g_pFontKor16, 30, 190, "환자가 침대에서 편안한");
	draw_text_kr(g_pFontKor16, 30, 220, "위치에 있는지 확인해");
	draw_text_kr(g_pFontKor16, 30, 250, "주세요.");

	// 확인 안내 박스
	draw_roundrectfill(24, 320, 272, 110, 10, MAKE_COLORREF(30, 30, 30));
	egl_font_set_color(g_pFontKor16, MAKE_COLORREF(255, 255, 255));
	draw_text_kr(g_pFontKor16, 46, 340, "[확인]을 3초간 누르세요");

	// 진행 바 — 10칸. 100ms 틱 기준 CONFORM_HOLD_TICKS(30 = 3초)으로 채운다.
	filled = (int)((conform_hold_cnt * CONFORM_BAR_SEGMENTS) / CONFORM_HOLD_TICKS);
	if(filled > CONFORM_BAR_SEGMENTS) filled = CONFORM_BAR_SEGMENTS;
	for(i = 0; i < CONFORM_BAR_SEGMENTS; i++){
		int cx = 50 + i * 24;
		if(i < filled)
			draw_roundrectfill(cx, 390, 14, 14, 7, MAKE_COLORREF(230, 150, 30));
		else
			draw_roundrectfill(cx, 390, 14, 14, 7, MAKE_COLORREF(70, 70, 70));
	}

	flip();
}

// 통신 끊김 화면 (spec §7) — 브로드캐스트 4.5초 미수신
void link_check_draw(void)
{
	set_draw_target(getbackframe());

	draw_rectfill(0, 0, 320, 480, MAKE_COLORREF(0, 0, 0));

	egl_font_set_color(g_pFontKor, MAKE_COLORREF(220, 220, 220));
	draw_text_kr(g_pFontKor, 62, 200, "연결 확인 중");

	egl_font_set_color(g_pFontKor16, MAKE_COLORREF(140, 140, 140));
	draw_text_kr(g_pFontKor16, 60, 270, "잠시만 기다려 주세요");

	flip();
}

// 설정 모드 표시 바 (임시 비활성)
static void draw_mode_indicator(void)
{
	// 오버레이 비활성
}

// 선택 부위 하이라이트 테두리 (임시 비활성)
static void draw_body_highlight(void)
{
	// 오버레이 비활성
}

//////////////////////////////////////////////////////////////////////////////
// SMART BED REMOCON portinit
//////////////////////////////////////////////////////////////////////////////
void smart_bed_remocon_port_init(void)
{
// P0.7 -> GPIO_IN : Not USED
// P0.6 -> S FLASH
// P0.5 -> S FLASH
// P0.4 -> S FLASH
// P0.3 -> GPIO IN : USER MODE
// P0.2 -> GPIO_IN : USER MODE
// P0.1 -> GPIO IN :  Not USED
// P0.0 -> GPIO IN : Not USED
	*R_PAF0 = F_PAF0_7_GP7 | F_PAF0_6_SFLASH_DQ2 | F_PAF0_5_SFLASH_DQ1 | F_PAF0_4_SFLASH_CS | 
					 F_PAF0_3_GP3 | F_PAF0_2_GP2 | F_PAF0_1_GP1 | F_PAF0_0_GP0;
	
// P1.7 -> NAND_nBUSY
// P1.6 -> NAND_nRE
// P1.5 -> NAND_nWE
// P1.4 -> NAND_CLE
// P1.3-> NAND ALE
// P1.2-> NAND CS
// P1.1 -> GPIO OUT : Not USED
// P1.0 -> UART TX0(Debug mode)
	*R_PAF1 = F_PAF1_7_NF_BUSYX | F_PAF1_6_NF_RE | F_PAF1_5_NF_WE | F_PAF1_4_NF_CLE | F_PAF1_3_NF_ALE | 
					 F_PAF1_2_NF_CS | F_PAF1_1_GP1 | F_PAF1_0_UART_TX0;
	*R_GPODIR(1) = (1<<1);// SD Card Detector LED
	
	*R_PAF2 = 0x0000; //nand data
	
// P3.7 -> GPIO OUT : KEY READ CS1
// P3.6 -> GPIO OUT : KEY READ LATCH1
// P3.5 -> TIMER OUT2 : LCD BACKLIGHT PWM
// P3.4 -> GPIO OUT : LED
// P3.3 -> GPIO OUT : LED
// P3.2 -> GPIO OUT ; LED
// P3.1 -> GPIO OUT : LED
// P3.0 -> GPIO OUT : LED
	*R_PAF3 = F_PAF3_7_GP7  | F_PAF3_6_GP6 | F_PAF3_5_GP5 | F_PAF3_4_GP4 | F_PAF3_3_GP3 |
					 F_PAF3_2_GP2 | F_PAF3_1_GP1 | F_PAF3_0_GP0;
	*R_GPPUEN(3) = (1<<7) | (1<<6) ;
	*R_GPODIR(3) = (1<<7) | (1<<6) | (1<<5) | (1<<4) | (1<<3) | (1<<2) | (1<<1) | (1<<0);
	*R_GPOLOW(3) = (1<<5);// LCD Backlingh Off
	*R_GPOHIGH(3) = (1<<7) | (1<<6) | (1<<4) |(1<<3) | (1<<2) | (1<<1) | (1<<0);
	
	
// P4.7 -> GPIO OUT : LCD CS
// P4.6 -> SPI SCK1 : LCD, KEY READ
// P4.5 -> GPIO OUT : Not USED
// P4.4 -> GPIO OUT : LED
// P4.3 -> UART_RX1
// P4.2 -> UART_TX1
// P4.1 -> GPIO OUT : KEY READ CS 0
// P4.0 -> GPIO OUT : KEY READ LATCH 0
	*R_PAF4 = F_PAF4_7_GP7 | F_PAF4_6_SPI_SCK1 | F_PAF4_5_GP5 | F_PAF4_4_GP4 | 
					 F_PAF4_3_UART_RX1 | F_PAF4_2_UART_TX1 | F_PAF4_1_GP1 | F_PAF4_0_GP0;	
	*R_GPPUEN(4) = (1<<7) | (1<<1) | (1<<0);	
	*R_GPODIR(4) =  (1<<7) | (1<<5) | (1<<4) | (1<<1) | (1<<0);
	*R_GPOHIGH(4) = (1<<7) | (1<<4) | (1<<1) | (1<<0);
	
// P5.7 -> LCD DOT CLOCK : LCD
// P5.6 -> DISP ENABLE : LCD
// P5.5 -> HSYNC : LCD
// P5.4 -> VSYNC : LCD
// P5.3 -> GPIO : LCD RESET
// P5.2 -> GPIO : LCD RS
// P5.1 -> SPI MOSI1 : LCD
// P5.0 -> SPI MISO1 : KEY
	*R_PAF5 = F_PAF5_7_CRTC_CLK_OUT | F_PAF5_6_DISP_EN | F_PAF5_5_HSYNC | F_PAF5_4_VSYNC | 
					 F_PAF5_3_GP3 | F_PAF5_2_GP2 | F_PAF5_1_SPI_MOSI1 | F_PAF5_0_SPI_MISO1;
	*R_GPPUEN(5) = (1<<3) | (1<<2);	
	*R_GPODIR(5) = (1<<3) | (1<<2);		 
	*R_GPOHIGH(5) = (1<<2)| (1<<3);
	//*R_GPOLOW(5) = (1<<3);
	
	// *R_PAF5 = F_PAF5_7_CRTC_CLK_OUT | F_PAF5_6_DISP_EN | F_PAF5_5_GP5 | F_PAF5_4_GP4 | 
					 // F_PAF5_3_GP3 | F_PAF5_2_GP2 | F_PAF5_1_SPI_MOSI1 | F_PAF5_0_SPI_MISO1;
	// *R_GPPUEN(5) = (1<<3) | (1<<2);	
	// *R_GPODIR(5) = (1<<5) | (1<<4) | (1<<3) | (1<<2);		 
	// *R_GPOHIGH(5) = (1<<5)| (1<<4);
	
// P6 -> RED[7:0]
	*R_PAF6 = 0x0000;
	
// P[7:3] -> GREEN[7:3]
	*R_PAF7 = 0x0000;
	
// P8.7 -> B7
// P8.6 -> B6
// P8.5 -> B5
// P8.4 -> B4
// P8.3 -> B3
// P8.2 -> GPIO OUT : USB nDISABLE
// P8.1 -> GPIO OUT : USB Detection
// P8.0 -> GPIO OUT : Not USED
	*R_PAF8 = F_PAF8_2_GP2 | F_PAF8_1_GP1 | F_PAF8_0_GP0;
	*R_GPODIR(8) = (1<<2) | (1<<0);
	*R_GPOLOW(8) = (1<<2) | (1<<0);	
	
// P9 -> Serial Flash
	*R_PAF9 =F_PAF9_2_SFLASH_DQ3 | F_PAF9_1_SFLASH_CLK | F_PAF9_0_SFLASH_DQ0;
	
}

int count_digits_string(int num)
{
	U8 str[100] = {0, };
	
	sprintf(str, "%d", num);
	return strlen(str);
}

 /////////////////////////////////////////////////////////////////////////////////////////////////
// booting mode 확인 함수
// GPIO0.2, GPIO0.3
// 11xx : Normal boot
// 10xx : 매뉴얼 모드
// 01xx : Masstorage Mode
//////////////////////////////////////////////////////////////////////////////////////////////////
U8 get_smart_bed_remocon_boot_mode(void)
{
	U8 boot_mode;
	
	boot_mode = *R_GPILEV(0);
	return boot_mode&((1<<3) | (1<<2));
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// USB DETECTION
// PORT : GPIO8 1
// Detection : LOW
// NO : LOW
// return : true   -> detection
// return : false -> non detection
//////////////////////////////////////////////////////////////////////////////////////////////////
bool usb_get_detection(void)
{
	int level_cnt = 0;
	U8 usb_level = 0, usb_level1 = 0;
	
	while(1){
		usb_level = (*R_GPILEV(8) & 0x02);
		if(usb_level != usb_level1)
		{
			usb_level1 = usb_level;
			level_cnt = 0;
		}
		else{
			level_cnt++;
			if(level_cnt >20){
				if(usb_level == 0x02) return false;
				return true;
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////////////////////////////
//
// LED OUT
//
//////////////////////////////////////////////////////////////////////////////////////////////////
// P3.0, P3.1, P3.2
#define test_led	// tera
void heat_led_ctrl(U8 led){
	// All OFF first
	*R_GPOHIGH(3) = (1<<0);
	*R_GPOHIGH(3) = (1<<1);
	*R_GPOHIGH(3) = (1<<2);

	// 누적 점등: 왼쪽(P3.2)부터 오른쪽(P3.0)으로
	if(led >= 1)
		*R_GPOLOW(3) = (1<<2);  // 왼쪽 LED
	if(led >= 2)
		*R_GPOLOW(3) = (1<<1);  // 가운데 LED
	if(led >= 3)
		*R_GPOLOW(3) = (1<<0);  // 오른쪽 LED
}
// P3.3, P3.4, P4.4
void volume_led_ctrl(U8 led){
	// All OFF first
	*R_GPOHIGH(3) = (1<<3);
	*R_GPOHIGH(3) = (1<<4);
	*R_GPOHIGH(4) = (1<<4);

	// 누적 점등: 왼쪽(P3.4)부터 오른쪽(P4.4)으로
	if(led >= 1)
		*R_GPOLOW(3) = (1<<4);  // P3.4 (왼쪽)
	if(led >= 2)
		*R_GPOLOW(3) = (1<<3);  // P3.3 (가운데)
	if(led >= 3)
		*R_GPOLOW(4) = (1<<4);  // P4.4 (오른쪽)
}

SURFACE* mass_image;
extern EGL_FONT* g_pFontbit;

// 교대부양 초기화
void levitate_value_power_init(void)
{
	int i, j;
	
	// debugprintf("\n\r ------- leviate init -----");
	for(i = BODY_HEAD; i < LEVIT_MAX; i++)// 일반/집중/수면
	{
		for(j = 0; j < BODY_MAX; j++)// 신체부위
		{
			levitate[i].body[j][BODY_ENABLE] = 0x10 * (j+1);// ((머리, 상체부, 하체부, 다리)Enable/Disable
			levitate[i].body[j][BODY_TIME + 1] = 0x30 ;	// 시간 (3단계)
			levitate[i].body[j][BODY_HIGH + 1] = 0x30 ;	// 높이 (3단계)
			levitate[i].body[j][BODY_RPM + 1] = 0x30 ;	// 속도 (3단계)
			// debugprintf("\n\r levitate[%d].body[%d] = 0x%x, 0x%x, 0x%x, 0x%x", 
				// i,j, levitate[i].body[j][BODY_ENABLE], levitate[i].body[j][BODY_TIME + 1],levitate[i].body[j][BODY_HIGH + 1], levitate[i].body[j][BODY_RPM + 1]);
		}
	}
}
// 체압분산 초기화
// body[i][0] = 부위 ID + Enable bit0 (ESP32: HeadOff=0x10, UpperBodyOn=0x21, ...)
// body[i][1] = pressure_level (0x10~0x60)
// body[i][2] = speed_level (0x10~0x60)
void dispersion_value_power_init(void)
{
	int i;
	// 머리는 고정 매트리스라 비활성, 나머지 활성
	U8 body_id[] = {0x10, 0x21, 0x31, 0x41};  // Head OFF, Upper ON, Lower ON, Leg ON

	for(i = 0; i < BODY_MAX; i++)
	{
		dispersion.body[i][0] = body_id[i];   // 부위 ID + Enable
		dispersion.body[i][1] = 0x30;          // pressure_level (3단계=100%)
		dispersion.body[i][2] = 0x30;          // speed_level (3단계=기존동작)
		dispersion.body[i][3] = 0x00;          // 예약 (ESP32에서 무시)
	}
}
// 마사지 초기화
void massage_value_power_init(void)
{
	int i,j;
	//debugprintf("\n\r ------- massage init -----");
	for(i = 0; i < MASSAGE_MAX; i++)// 일반/집중/수면
	{
		for(j = 0; j < BODY_MAX; j++)// 신체부위
		{
			massage[i].body[j][BODY_ENABLE] = 0x10 * (j+1);// (머리, 상체부, 하체부, 다리)Enable/Disable
			massage[i].body[j][BODY_TIME + 1] = 0x30 ;	// 시간 (3단계)
			massage[i].body[j][BODY_HIGH + 1] = 0x30 ;	// 높이 (3단계)
			massage[i].body[j][BODY_RPM + 1] = 0x30 ;	// 속도 (3단계)
			// debugprintf("\n\r massage[%d].body[%d] = 0x%x, 0x%x, 0x%x, 0x%x", 
				// i,j, massage[i].body[j][BODY_ENABLE], massage[i].body[j][BODY_TIME + 1],massage[i].body[j][BODY_HIGH + 1], massage[i].body[j][BODY_RPM + 1]);
		}
	}	
}
// 환자케어 초기화
void patient_care_value_power_init(void)
{
	memset(&patient_care, 0, sizeof(BODY));
}

void heat_value_power_init(void)
{
	memset(&heat, 0, sizeof(BODY));
}

void volume_value_power_init(void)
{
	memset(&volume, 0, sizeof(BODY));
	volume.body[0][0] = 1;  // 볼륨 기본값: LOW (25)
	volume_led_ctrl(1);
}
//////////////////////////////////////////////////////////////////////////////////////////////////
// up key로 이동을 수행한다.
// 설정/저장 키를 눌러야만 저장 한다.
//////////////////////////////////////////////////////////////////////////////////////////////////
void up_key_proc(void)
{
	switch(cursor.mode)
	{
		case CURSOR_BODY: // 머리, 상체부, 하체부, 다리
			if(cursor.body) cursor.body--;
			else cursor.body = BODY_LEG;
			smart_bed_display.display_refresh = true;// lcd refresh
			break;
			
		case CURSOR_SET:// 화면 우측 설정값
			if(cursor.set) cursor.set--;
			else cursor.set = cursor.set_max;
			smart_bed_display.display_refresh = true;// lcd refresh
			break;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// down key로 이동을 수행한다.
// 설정/저장 키를 눌러야만 저장 한다.
//////////////////////////////////////////////////////////////////////////////////////////////////
void down_key_proc(void)
{
	switch(cursor.mode)
	{
		case CURSOR_BODY :// 머리, 몸체 상, 몸체 하, 다리
			cursor.body++;
			if(cursor.body >= BODY_MAX)
				cursor.body = BODY_HEAD;
			smart_bed_display.display_refresh = true;// lcd refresh
			break;
			
		case CURSOR_SET://  화면 우측 설정값
				cursor.set++;
			if(cursor.set >= cursor.set_max)
				cursor.set = 0;
			smart_bed_display.display_refresh = true;// lcd refresh
			break;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// 설정/저장 방향으로 이동을 수행한다.
// 설정/저장 키를 누르면 첫 번째 키를 눌러야 설정값 쪽으로 이동한다.
// 설정값 쪽으로 이동 후 값 감소로 된다.
//
//////////////////////////////////////////////////////////////////////////////////////////////////
void left_key_proc(bool left, BODY *sub_title)
{
	switch(cursor.mode)
	{
		case CURSOR_BODY :// Enable/Disable 토글
			temp_body.body[cursor.body][BODY_ENABLE] ^= 0x01;
			smart_bed_display.display_refresh = true;
			debugprintf("\n\rLEFT: TOGGLE [0x%x]", temp_body.body[cursor.body][BODY_ENABLE]);
			break;
			
		case CURSOR_SET:// 설정값 감소 (--)
			if(temp_body.body[cursor.body][cursor.set + 1] >= 0x20)
				temp_body.body[cursor.body][cursor.set + 1] -= 0x10;
			smart_bed_display.display_refresh = true;// lcd refresh
			break;
			
		default :
			if(left == true)
			{// 커서 상하 이동(메인 설정값 쪽)
				if(cursor.type)
				{
					cursor.type--;
				}
				else
				{
					cursor.type = cursor.type_max-1;
				}
				memcpy(&temp_body, &sub_title[cursor.type], sizeof(BODY));
				smart_bed_display.display_refresh = true;// lcd refresh
			}
			conform_key_run = 0;
	}
}

void left_key_proc1(void)
{
	if(cursor.type)
	{
		cursor.type--;
	}
	else
	{
		cursor.type = cursor.type_max-1;
		debugprintf("cur type[%d]\n\r", cursor.type);
	}
	smart_bed_display.display_refresh = true;// lcd refresh
	conform_key_run = 0;
}		

//////////////////////////////////////////////////////////////////////////////////////////////////
// 설정/저장 방향으로 이동을 수행한다.
// 설정/저장 키를 누르면 첫 번째 키를 눌러야 설정값 쪽으로 이동한다.
// 설정값 쪽으로 이동 후 값 증가로 된다.
// right옵션이 1인경우 상위 메뉴 커서를 좌우 이동 시킨다.
//////////////////////////////////////////////////////////////////////////////////////////////////
void right_key_proc(bool right, BODY *sub_title)
{
	switch(cursor.mode)
	{
		case CURSOR_BODY :// Enable/Disable 토글
			temp_body.body[cursor.body][BODY_ENABLE] ^= 0x01;
			smart_bed_display.display_refresh = true;
			debugprintf("\n\rRIGHT: TOGGLE [0x%x]", temp_body.body[cursor.body][BODY_ENABLE]);
			break;
			
		case CURSOR_SET:// 설정값 증가
			if(temp_body.body[cursor.body][cursor.set + 1] < body_set_max[cursor.set])
				temp_body.body[cursor.body][cursor.set + 1] += 0x10;
			smart_bed_display.display_refresh = true;// refresh
			break;
			
		default :// 일반, 집중, 수면
			if(right == true)
			{
				cursor.type++;
				if(cursor.type >= cursor.type_max)
					cursor.type = 0;
				smart_bed_display.display_refresh = true;// lcd refresh
				//memcpy(&temp_body, sub_title, sizeof(BODY));
				memcpy(&temp_body, &sub_title[cursor.type], sizeof(BODY));
			}
			conform_key_run = 0;
	}
}

void right_key_proc1(void)
{
	cursor.type++;
	if(cursor.type >= cursor.type_max)
		cursor.type = 0;
	debugprintf("cur type[%d]\n\r", cursor.type);
	smart_bed_display.display_refresh = true;// lcd refresh
	conform_key_run = 0;
}

// 설정 및 저장 키
// NORMAL -> 설정 -> 저장 -> NORMAL
// 환자케어는 저장 키를 사용하지 않으므로 set은 0부터
// 전체 부위를 한번에 저장하고 ESP32로 전송
void set_key_proc(U8 cmd_act, BODY *dest)
{
	int i, j;

	if(cursor.mode == CURSOR_MODE)
	{
		cursor.mode = CURSOR_BODY;
		cursor.body = 0;
		cursor.set = 0;
		smart_bed_display.display_refresh = true;// lcd refresh
	}
	else
	{
		cursor.mode = CURSOR_MODE;
		// 전체 부위 설정을 원본에 복사
		memcpy(dest, &temp_body, sizeof(BODY));
		// 전체 부위 ESP32 전송
		for(i = 0; i < BODY_MAX; i++)
		{
			debugprintf("\n\rbody[%d]: ", i);
			for(j = 0; j < BODY_LEVIT_MAX + 1; j++)
				debugprintf("0x%2x ", dest->body[i][j]);
			esp32_packet_send(CMD1_SEND_SET_VAL, cmd_act, dest->body[i], BODY_LEVIT_MAX + 1);
		}
		smart_bed_display.display_refresh = true;// lcd refresh
		conform_key_run = 0;
	}
}

//
// 확인 키(CONFORM KEY)
// 확인키는 두가지 MODE가 있다.
// 1. NORMAL MODE: START, PAUSE, RESTART
// 2. SET MODE : Enalbe/Disable용
// 
void conform_key_proc(U8 cmd_act)
{
	switch(cursor.mode)
	{
		case CURSOR_MODE:	// start/pause/restart
			// bed_status.run_state로 conform_key_run 보정 — 모드가 자연 종료(시간 종료)되거나
			// 외부에서 종료된 경우 stale CMD3_PAUSE/RESTART를 START로 리셋
			if(bed_status.run_state == 0) {
				conform_key_run = CMD3_START;
				running_massage_type = -1;	// 자연 종료 후 stale 인덱스 클리어
			}
			if(conform_key_run == CMD3_START)
			{// 최초 시작
				// [모드→모드 전환 감지] bed_status 덮어쓰기 전에 직전 상태로 판정.
				//   직전에 다른 모드가 동작 중이었으면 전환 홈이 필요 → 홈 동안 "종료중",
				//   대기(건반 홈 완료)에서 시작이면 즉시 모드명/카운트.
				mode_switch_from_running = (bed_status.run_state != 0 && bed_status.current_mode != 0);
				// 마사지가 아닌 다른 모드 시작 시 running_massage_type 클리어
				if(cmd_act < CMD2_MASSAGE || cmd_act >= (CMD2_MASSAGE + MASSAGE_MAX))
					running_massage_type = -1;
				esp32_packet_send(CMD1_SEND_RUN_ST, cmd_act, &conform_key_run , 1);
				// 새 모드 시작 → 이전 stdby 진행 취소 (사용자가 명백히 새 동작 선택)
				if(stdby_in_progress) {
					stdby_in_progress = false;
					stdby_complete = false;
					debugprintf("\n\r [CONFORM] stdby canceled by new mode start");
				}
				// 즉시 로컬 상태 반영 + 잠금
				bed_status.current_mode = cmd_act;
				bed_status.run_state = 1;
				bed_state_lock_remain = 100;
				// ESP32 홈(init) 진행 중 표시 — 경과시간 정지 + "초기화 중" 텍스트.
				// ESP32 처리 sequence:
				//   ① CONFORM 처리 직후 (mode, state=1) 즉시 브로드캐스트 (pre-homing)
				//   ② 호밍 진행: is_homing=true → (mode, 3) 브로드캐스트 (혹은 fast homing이면 스킵)
				//   ③ 홈 완료(BarsInitialized) → (mode, state=1) 즉시 브로드캐스트 (post-homing)
				// state=1 packet 개수 세서 2번째에 클리어 → 홈 시간 무관하게 안정적 동작.
				init_home_pending = true;
				init_home_pending_timeout = 600;	// 60초 안전 timeout
				init_home_state1_count = 0;
				conform_key_run = CMD3_PAUSE;
				// 모드 실행 후 홈화면으로 전환 (상태 오버레이로 동작 표시)
				smart_bed_status.status = MODE_HOME;
				smart_bed_display.status = MODE_HOME;
				smart_bed_display.display_refresh = true;
			}
			else
			{
				if(conform_key_run == CMD3_PAUSE)
				{//일시정지
					esp32_packet_send(CMD1_SEND_RUN_ST,CMD2_PAUSE, &conform_key_run, 1);
					bed_status.run_state = 2;	// 즉시 일시정지 상태 반영
					bed_state_lock_remain = 100;
					smart_bed_display.display_refresh = true;
					conform_key_run = CMD3_RESTART;
				}
				else
				{// 재 시작
					esp32_packet_send(CMD1_SEND_RUN_ST,CMD2_RESTART, &conform_key_run, 1);
					bed_status.run_state = 1;	// 즉시 동작중 상태 반영
					bed_state_lock_remain = 100;
					smart_bed_display.display_refresh = true;
					conform_key_run = CMD3_PAUSE;
				}
			}
			break;
			
		case CURSOR_BODY :// 부위 선택 → 값 조정 모드 진입
			cursor.mode = CURSOR_SET;
			cursor.set = 0;
			smart_bed_display.display_refresh = true;
			debugprintf("\n\rCONFORM: BODY -> SET");
			break;
		case CURSOR_SET :// 값 조정 → 부위 선택 모드 복귀
			cursor.mode = CURSOR_BODY;
			smart_bed_display.display_refresh = true;
			debugprintf("\n\rCONFORM: SET -> BODY");
			break;
	}
}

void conform_key_proc1(U8 cmd_act)
{
	U8 tmp = 0;
	running_massage_type = -1;	// 환자케어 시작 시 마사지 상태 클리어

	if(temp_body.body[0][0] != (cursor.type + 1))
	{
		temp_body.body[0][0] = cursor.type + 1;
		tmp = 1;
		esp32_packet_send(CMD1_SEND_RUN_ST, cmd_act, &tmp, 1);
		// 새 모드 시작 → 이전 stdby 진행 취소
		if(stdby_in_progress) {
			stdby_in_progress = false;
			stdby_complete = false;
			debugprintf("\n\r [CONFORM1] stdby canceled by new mode start");
		}
		// 즉시 로컬 상태 반영 + 잠금
		bed_status.current_mode = cmd_act;
		bed_status.run_state = 1;
		bed_state_lock_remain = 100;
	}
	else
	{
		temp_body.body[0][0] = 0;
		esp32_packet_send(CMD1_SEND_RUN_ST, cmd_act, &tmp, 1);
		// 종료 — 즉시 mode 클리어 + 잠금
		bed_status.current_mode = 0;
		bed_status.run_state = 0;
		bed_state_lock_remain = 100;
	}
	smart_bed_display.display_refresh = true;// lcd refresh
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//
// 체압분산
// KEY : up, down, left, right, 확인, 설정/저장, 초기화, 온열, 음량
// 동작 중  체압분산 키를 다시누르면 동작을 종료하고 현재 값을 저장후 된다.
// pressure value(0~255) -> 연속 그라데이션 색상
//   0       : 빈 셀 (회색)
//   1~84    : 파랑(저) → 녹색(중)
//   85~169  : 녹색(중) → 주황(고)
//   170~255 : 주황(고) → 빨강(위험)
static U32 interp_rgb(int r1, int g1, int b1, int r2, int g2, int b2, int t, int t_max)
{
	int r = r1 + (r2 - r1) * t / t_max;
	int g = g1 + (g2 - g1) * t / t_max;
	int b = b1 + (b2 - b1) * t / t_max;
	if(r < 0) r = 0; if(r > 255) r = 255;
	if(g < 0) g = 0; if(g > 255) g = 255;
	if(b < 0) b = 0; if(b > 255) b = 255;
	return MAKE_COLORREF(r, g, b);
}

U32 pressure_to_color(U8 value)
{
	if(value == 0) return MAKE_COLORREF(150, 152, 158);	// 빈 셀
	if(value < 85)
		return interp_rgb( 60, 100, 220,  80, 200,  80, value, 84);
	if(value < 170)
		return interp_rgb( 80, 200,  80, 255, 160,  50, value - 85, 84);
	return     interp_rgb(255, 160,  50, 220,  60,  60, value - 170, 85);
}

// 모터 위치 → 측면뷰 셀 y-offset (위로 이동량)
//   0     → 0px (원위치)
//   1260  → 28px (구분선↔바 스트립 33px gap의 중앙)
//   3400+ → 30px (gap 중앙에서 추가로 2px 더 위)
int motor_position_to_offset(S16 pos)
{
	if(pos <= 0) return 0;
	if(pos <= 1260) return ((int)pos * 28) / 1260;
	if(pos <= 3400) return 28 + ((int)(pos - 1260) * 2) / (3400 - 1260);
	return 30;
}

// 체압분산 관련된 키 입력을 체크하는 루틴
// 체압분산 모드에서 체압분산 키를 다시 누르면은 홈화면으로 전환된다.
// 설정/저장 키를 누르지않은 상태에서는 UP/DOWN/LEFT/RIGHT를 누르지는 않는다.
// 신체를 선택후 UP/DOWN으로 선택하고 설정 변경은 LEFT/RIGHT로 한다.
// 선택확인은 확인 버튼으로 한다.
//
//////////////////////////////////////////////////////////////////////////////////////////////////
void dispersion_proc(void)
{
	if(smart_bed_display.status != MODE_VAIRANCE)
	{
		smart_bed_display.status = MODE_VAIRANCE;	// 교대부양 display 갱신
		smart_bed_display.display_refresh = true;// 체압분산 display
		memset(&cursor, 0, sizeof(CURSOR));// key init
		cursor.set_max = 2;// 커서표시 개수
		cursor.type_max = 0;
		memcpy(&temp_body, &dispersion, sizeof(BODY));// 현재 설정 임시 버퍼로 복사
		
		remocon_key.key_val = 0xFF;
		body_set_max[0] = 0x60;// LEVEL
		body_set_max[1] = 0x60;	// 몸체(동작속도)
		conform_key_run = 0;
		return;
	}
	
	switch(remocon_key.key_val){
		case ULCER_KEY:
			smart_bed_status.status = MODE_HOME;
			remocon_key.key_val = 0xFF;
			break;
		case CONFORM_KEY:
			conform_key_proc(CMD2_DISPERSION);
			remocon_key.key_val = 0xFF;
			break;
		case UP_KEY:
		case DOWN_KEY:
		case LEFT_KEY:
		case RIGHT_KEY:
			remocon_key.key_val = 0xFF;
			break;
		default:
			remocon_key.key_val = 0xFF;
	}
}

// PDF 슬라이드 8 — 체압분산 (단일 모드, 큰 노란 박스)
void dispersion_draw(void){
	char buf[8];
	U32 BORDER = MAKE_COLORREF(220, 50, 50);
	U32 icon_color;
	U32 pill_bg;

	set_draw_target(getbackframe());
	draw_rectfill(0, 0, 320, 480, MAKE_COLORREF(20, 25, 38));

	// 헤더 (공용 — 온도/음량/무중력)
	draw_status_header();

	// 체압분산 타이틀 (청록색)
	draw_roundrectfill(5, 55, 310, 50, 8, MAKE_COLORREF(80, 180, 200));
	egl_font_set_color(g_pFontKor, MAKE_COLORREF(255, 255, 255));
	draw_text_kr(g_pFontKor, 104, 66, "체압분산");

	// 노란 박스 (단일 모드) — 전체 폭으로 확장 (타이틀 바와 동일)
	draw_roundrectfill(5, 224, 310, 98, 8, MAKE_COLORREF(255, 230, 80));
	egl_font_set_color(g_pFontKor, MAKE_COLORREF(20, 25, 38));
	draw_text_kr(g_pFontKor, 104, 259, "체압분산");

	// 푸터
	egl_font_set_color(g_pFontKor16, MAKE_COLORREF(200, 200, 200));
	draw_text_kr(g_pFontKor16, 70, 460, "체압분산 버튼 : 홈으로");

	flip();
}

void dispersion_draw_legacy(void){
	int i = 0, num = 0, digits = 0;
	char string[100] = {0, };

	set_draw_target(getbackframe());// back frame select
	draw_surface(btm1_img,0,0);	// 배경 이미지(의미 없음)
	draw_surface(vaira_title_imag, TITLE_X, TITLE_Y);	// title 이미지
	draw_surface(vaira_main_icon_imag, MAIN_ICON_X, MAIN_ICON_Y);	// 메인 아이콘

// 설정 모드 표시 바
	draw_mode_indicator();

// BAR draw (동작 중 매트리스 표시)
	for(i = 0; i < MAX_BAR; i++)
	{
		if( bar[i] )
			draw_surface(bar_img, BAR_X, (BAR_Y + BAR_I * i));
	}
// 인체 이미지 draw
	if(body_info == true)
		draw_surface(body_imag, BODY_X, BODY_Y);

// 7x10 pressure heatmap (임시 비활성)
#if 0
	{
		int row, col;
		for(row = 0; row < PRESSURE_ROWS; row++)
		{
			for(col = 0; col < PRESSURE_COLS; col++)
			{
				draw_rectfill(
					HEATMAP_X + col * (HEATMAP_CELL_W + HEATMAP_GAP),
					HEATMAP_Y + row * (HEATMAP_CELL_H + HEATMAP_GAP),
					HEATMAP_CELL_W,
					HEATMAP_CELL_H,
					pressure_to_color(pressure_map[row][col])
				);
			}
		}
	}
#endif

// 머리, 상체부, 하체부, 다리
	for(i = 0; i < BODY_MAX; i++)
	{
		if(cursor.mode != CURSOR_MODE)
		{// 설정 모드: 커서 선택 표시
			if(cursor.body == i)
				draw_surface(radio_avail_selt_img,radio_xy[i][0], radio_xy[i][1]);// 선택됨
			else
			{
				if(temp_body.body[i][0] & 0x01)
					draw_surface(radio_avail_img,radio_xy[i][0], radio_xy[i][1]);// Enable
				else
					draw_surface(radio_dis_selt_img,radio_xy[i][0], radio_xy[i][1]);// Disable
			}
		}
		else
		{// 일반 모드: Enable/Disable 상태만 표시
			if(temp_body.body[i][0] & 0x01)
				draw_surface(radio_avail_img,radio_xy[i][0], radio_xy[i][1]);// Enable
			else
				draw_surface(radio_dis_selt_img,radio_xy[i][0], radio_xy[i][1]);// Disable
		}
	}
// 선택 부위 하이라이트 테두리
	draw_body_highlight();

// 설정 값 표시 : 레벨, 동작속도
	egl_font_set_color(g_pFont28, MAKE_COLORREF(255,255,255));
	for(i = 0; i < cursor.set_max; i++)
	{
		if(cursor.mode == CURSOR_SET)
		{
			if(cursor.set == i)
				draw_surface(dip_en_num_img, num2_xy[i][0], num2_xy[i][1]);// Enable 활성
			else
				draw_surface(dip_ds_num_img, num2_xy[i][0], num2_xy[i][1]);// Disable 비활성
		}
		else
		{
			draw_surface(dip_ds_num_img, num2_xy[i ][0], num2_xy[i ][1]);// Disable 비활성
		}
		debugprintf("\n\r -> 0x%x", temp_body.body[cursor.body][i+1]);
		num = (temp_body.body[cursor.body][i+1]>>4);
		
		sprintf(string, "%d", num);
		// digits = count_digits_string(num);
		// bmpfont_draw(g_pFont40, num2_xy[i ][0]+(3-1)*10 , num2_xy[i ][1]+10, string);
		digits = count_digits_string(num);
		switch(digits)
		{
			case 1:
				bmpfont_draw(g_pFont28, num2_xy[i ][0]+ 22, num2_xy[i ][1]+11, string);
				break;
			case 2:
				bmpfont_draw(g_pFont28, num2_xy[i ][0]+13, num2_xy[i ][1]+11, string);
				break;
			default:
				bmpfont_draw(g_pFont28, num2_xy[i ][0]+(4-digits) *8, num2_xy[i ][1]+11, string);
		}
	}
	flip();
}

// ====================교대 부양 START=============================== //
//////////////////////////////////////////////////////////////////////////////////////////////////
//
// 교대 부양
// 1. LEFT, RIGHT 버튼으로 일반->집중->수면으로 이동 가능함
// 2. 교대부양 키를 다시 누르면 홈 화면으로 돌아간다.
// 3. 설정/저장으로 저장.
//
//////////////////////////////////////////////////////////////////////////////////////////////////
void levitate_proc(void)
{
	if(smart_bed_display.status != MODE_LEVITATE)
	{// 처음 교대부양 진입 할 경우
		smart_bed_display.status = MODE_LEVITATE;	// 교대부양 display 갱신
		smart_bed_display.display_refresh = true;// 교대부양 display
		memset(&cursor, 0, sizeof(CURSOR));// key init
		cursor.set_max = BODY_LEVIT_MAX;
		cursor.type_max = 1;	// 집중/수면 삭제 → 일반(0x10)만 노출

		// 동작 중인 교대부양 모드가 있으면 해당 모드로 커서 이동 + 동작 상태 복원.
		// 교대부양 CMD: 0x10(표준=LEVIT_NOR)만 사용 (집중 0x11/수면 0x12 삭제됨)
		U8 m = bed_status.current_mode;
		if(m == CMD2_VENTIL_STD &&
		   bed_status.run_state != 0){
			cursor.type = 0;
			if(bed_status.run_state == 2)
				conform_key_run = CMD3_RESTART;
			else
				conform_key_run = CMD3_PAUSE;
		} else {
			conform_key_run = 0;
		}

		memcpy(&temp_body, &levitate[cursor.type], sizeof(BODY));// 현재 설정 임시 버퍼로 복사

		remocon_key.key_val = 0xFF;
		body_set_max[BODY_TIME] = 0xC0;// time
		body_set_max[BODY_HIGH] = 0x60;	// high
		body_set_max[BODY_RPM] = 0x30;// rpm
		return;
	}
	
	switch(remocon_key.key_val){
		case MASSAGE_KEY:
			smart_bed_status.status = MODE_HOME;
			remocon_key.key_val = 0xFF;
			break;
		case UP_KEY:		// 위 → 모드 선택 위로 이동
			if(cursor.type)
				cursor.type--;
			else
				cursor.type = cursor.type_max - 1;
			memcpy(&temp_body, &levitate[cursor.type], sizeof(BODY));
			conform_key_run = 0;
			smart_bed_display.display_refresh = true;
			remocon_key.key_val = 0xFF;
			break;
		case DOWN_KEY:		// 아래 → 모드 선택 아래로 이동
			cursor.type++;
			if(cursor.type >= cursor.type_max)
				cursor.type = 0;
			memcpy(&temp_body, &levitate[cursor.type], sizeof(BODY));
			conform_key_run = 0;
			smart_bed_display.display_refresh = true;
			remocon_key.key_val = 0xFF;
			break;
		case LEFT_KEY:		// 좌/우는 PDF 디자인에서 미사용
		case RIGHT_KEY:
			remocon_key.key_val = 0xFF;
			break;
		case CONFORM_KEY:// 확인(설정중에서는 확인키로 Enable/Disable
			if(cursor.type == 0)
				conform_key_proc(CMD2_VENTIL_STD);	// 일반
			else if(cursor.type == 1)
				conform_key_proc(CMD2_VENTIL_FOCUS);	// 집중
			else if(cursor.type == 2)
				conform_key_proc(CMD2_VENTIL_SLEEP);	// 수면
			remocon_key.key_val = 0xFF;
			break;
		case SET_KEY:
			if(cursor.type == 0)
				set_key_proc(CMD2_VENTIL_STD, &levitate[cursor.type]);	// 일반
			else if(cursor.type == 1)
				set_key_proc(CMD2_VENTIL_FOCUS, &levitate[cursor.type]);	// 집중
			else if(cursor.type == 2)
				set_key_proc(CMD2_VENTIL_SLEEP, &levitate[cursor.type]);	// 수면
			remocon_key.key_val = 0xFF;
			break;
		default:
			remocon_key.key_val = 0xFF;
	}
}





// PDF 슬라이드 6 — 교대부양 모드 선택 화면
void levitate_draw(void){
	U32 BORDER = MAKE_COLORREF(220, 50, 50);

	set_draw_target(getbackframe());
	// 단색 배경
	draw_rectfill(0, 0, 320, 480, MAKE_COLORREF(20, 25, 38));

	// ===== 헤더 (홈과 동일) =====
	draw_roundrect(5, 0, 138, 50, 8, BORDER);
	draw_roundrect(177, 0, 138, 50, 8, BORDER);

	// 모드 상태 (좌측) — get_mode_status_text 사용
	{
		U32 icon_color;
		switch(bed_status.run_state) {
			case 1: icon_color = MAKE_COLORREF( 80, 220, 100); break;
			case 2: icon_color = MAKE_COLORREF(255, 200,  50); break;
			case 3: icon_color = MAKE_COLORREF(120, 160, 240); break;
			default: icon_color = MAKE_COLORREF(255,  80,  80); break;
		}
		if(bed_status.run_state == 1)
			draw_pause_icon(10, 16, 18, icon_color);
		else if(bed_status.run_state == 2)
			draw_triangle_icon(10, 16, 18, icon_color);
		else
			draw_play_icon(10, 16, 18, icon_color);
		{
			CLIP_RECT new_clip, old_clip;
			new_clip.x = 32;
			new_clip.y = 0;
			new_clip.endx = 141;
			new_clip.endy = 50;
			old_clip = draw_set_clip_window(getbackframe(), &new_clip);
			egl_font_set_color(g_pFontKor, MAKE_COLORREF(255, 255, 255));
			draw_text_kr(g_pFontKor, 32 - (int)mode_text_offset, 11,
				get_mode_status_text(bed_status.current_mode, bed_status.run_state));
			draw_set_clip_window(getbackframe(), &old_clip);
		}
	}

	// 온도 + 음량 (우측)
	{
		char buf[8];
		U32 pill_bg;
		pill_bg = bed_status.heat_level > 0 ?
			MAKE_COLORREF(180, 90, 30) : MAKE_COLORREF(60, 60, 70);
		draw_roundrectfill(180, 2, 65, 46, 6, pill_bg);
		draw_thermo_icon(193, 11, MAKE_COLORREF(255, 255, 255));
		sprintf(buf, "%d", bed_status.heat_level);
		egl_font_set_color(g_pFont28, MAKE_COLORREF(255, 255, 255));
		bmpfont_draw(g_pFont28, 215, 11, buf);

		pill_bg = bed_status.volume_level > 0 ?
			MAKE_COLORREF(40, 100, 160) : MAKE_COLORREF(60, 60, 70);
		draw_roundrectfill(249, 2, 64, 46, 6, pill_bg);
		draw_speaker_icon(260, 12, 26, MAKE_COLORREF(255, 255, 255));
		sprintf(buf, "%d", bed_status.volume_level);
		bmpfont_draw(g_pFont28, 286, 11, buf);
	}

	// ===== 모드 선택 영역 =====
	// 타이틀 바 (녹색) — 시간 박스 자리로 올리고 살짝 키움 (h=38 → 50)
	draw_roundrectfill(5, 55, 310, 50, 8, MAKE_COLORREF(60, 160, 100));
	egl_font_set_color(g_pFontKor, MAKE_COLORREF(255, 255, 255));
	draw_text_kr(g_pFontKor, 104, 66, "교대부양");

	// 교대부양은 일반(0x10) 단일 모드 — 화면 중앙에 큰 버튼 1개 (집중/수면 삭제)
	// 타이틀(~105)과 푸터(460) 사이 세로 중앙에 배치, 텍스트 가로/세로 중앙 정렬
	{
		int btn_x = 20, btn_w = 280;
		int btn_h = 160;
		int btn_y = 200;	// (105+460)/2=282 기준 버튼 중앙 정렬
		char buf[40];
		int tw, tx;

		draw_roundrectfill(btn_x, btn_y, btn_w, btn_h, 12, MAKE_COLORREF(255, 230, 80));
		egl_font_set_color(g_pFontKor, MAKE_COLORREF(20, 25, 38));

		// 1행: 모드명, 2행: 동작 시간
		tw = estimate_text_width_28("일반모드");
		tx = btn_x + (btn_w - tw) / 2;
		draw_text_kr(g_pFontKor, tx, btn_y + 45, "일반모드");

		sprintf(buf, "[10초]");
		tw = estimate_text_width_28(buf);
		tx = btn_x + (btn_w - tw) / 2;
		draw_text_kr(g_pFontKor, tx, btn_y + 90, buf);
	}

	// 푸터 힌트 — 화면 맨 아래
	egl_font_set_color(g_pFontKor16, MAKE_COLORREF(200, 200, 200));
	draw_text_kr(g_pFontKor16, 70, 460, "교대부양 버튼 : 홈으로");

	flip();
}

// 기존 레거시 levitate_draw (체압 분포 + 라디오 + 설정 UI) — 현재 미사용
void levitate_draw_legacy(void){
	int i = 0, num = 0, digits = 0;
	char string[200] = {0, };

	set_draw_target(getbackframe());// back frame select
	draw_surface(btm1_img,0,0);	// 배경 이미지(의미 없음)
	draw_surface(levitate_title_img, TITLE_X, TITLE_Y);	// title 이미지
	draw_surface(levitate_main_icon_img, MAIN_ICON_X, MAIN_ICON_Y);	// 메인 아이콘

// 일반, 집중, 수면
	switch(cursor.type){
		case LEVIT_NOR:
			draw_surface(levitate_normal_sel_icon_img, LEVIT_NOR_X, LEVIT_SUB_Y);	// 일반
			draw_surface(levitate_concent_icon_img, LEVIT_CON_X, LEVIT_SUB_Y);	// 집중
			draw_surface(levitate_sleep_icon_img, LEVIT_SLE_X, LEVIT_SUB_Y);	// 수면
			break;
		case LEVIT_FOCUS:
			draw_surface(levitate_normal_icon_img, LEVIT_NOR_X, LEVIT_SUB_Y);	// 일반
			draw_surface(levitate_concent_sel_icon_img, LEVIT_CON_X, LEVIT_SUB_Y);	// 집중
			draw_surface(levitate_sleep_icon_img, LEVIT_SLE_X, LEVIT_SUB_Y);	// 수면
			break;
		case LEVIT_SLEEP:
			draw_surface(levitate_normal_icon_img, LEVIT_NOR_X, LEVIT_SUB_Y);	// 일반
			draw_surface(levitate_concent_icon_img, LEVIT_CON_X, LEVIT_SUB_Y);	// 집중
			draw_surface(levitate_sleep_sel_icon_img, LEVIT_SLE_X, LEVIT_SUB_Y);	// 수면
	}

// 설정 모드 표시 바
	draw_mode_indicator();

// BAR draw
	for(i = 0; i < MAX_BAR; i++)
	{
		if( bar[i] )
			draw_surface(bar_img, BAR_X, (BAR_Y + BAR_I * i));
	}
// 인체 이미지 draw
	if(body_info == true)
		draw_surface(body_imag, BODY_X, BODY_Y);	// body image

// 머리, 상체부, 하체부, 다리
	for(i = 0; i < BODY_MAX; i++)
	{
		if(cursor.mode != CURSOR_MODE)
		{// 설정 모드: 커서 선택 표시
			if(cursor.body == i)
				draw_surface(radio_avail_selt_img,radio_xy[i][0], radio_xy[i][1]);// 선택됨
			else
			{
				if(temp_body.body[i][0] & 0x01)
					draw_surface(radio_avail_img,radio_xy[i][0], radio_xy[i][1]);// Enable
				else
					draw_surface(radio_dis_selt_img,radio_xy[i][0], radio_xy[i][1]);// Disable
			}
		}
		else
		{// 일반 모드: Enable/Disable 상태만 표시
			if(temp_body.body[i][0] & 0x01)
				draw_surface(radio_avail_img,radio_xy[i][0], radio_xy[i][1]);// Enable
			else
				draw_surface(radio_dis_selt_img,radio_xy[i][0], radio_xy[i][1]);// Disable
		}
	}
// 선택 부위 하이라이트 테두리
	draw_body_highlight();

// 설정 값 표시 : 시간설정, 높이, 속도
	egl_font_set_color(g_pFont28, MAKE_COLORREF(255,255,255));
	for(i = 0; i < cursor.set_max; i++)
	{
		if(cursor.mode == CURSOR_SET)
		{
			if(cursor.set == i)
				draw_surface(dip_en_num_img, num3_xy[i][0], num3_xy[i][1]);// Enable 활성
			else
				draw_surface(dip_ds_num_img, num3_xy[i][0], num3_xy[i][1]);// Disable 비활성
		}
		else
		{
			draw_surface(dip_ds_num_img, num3_xy[i ][0], num3_xy[i ][1]);// Disable 비활성
		}
		debugprintf("\n\r -> 0x%x", temp_body.body[cursor.body][i+1]);

		if(i == 0) num = (temp_body.body[cursor.body][i+1]>>4) * 10;
		else
		{
			if(i == 1)
			{
				num = (temp_body.body[cursor.body][i+1]>>4);
			}
			else
			{
				num = (temp_body.body[cursor.body][i+1]>>4);
			}
		}
		sprintf(string, "%d", num);
		digits = count_digits_string(num);
		switch(digits)
		{
			case 1:
				bmpfont_draw(g_pFont28, num3_xy[i ][0]+ 22, num3_xy[i ][1]+11, string);
				break;
			case 2:
				bmpfont_draw(g_pFont28, num3_xy[i ][0]+13, num3_xy[i ][1]+11, string);
				break;
			default:
				bmpfont_draw(g_pFont28, num3_xy[i ][0]+(4-digits) *8, num3_xy[i ][1]+11, string);
		}
	}
	flip();
}
// =====================교대 부양 END ============================== //

// ====================== 마사지 Start ===============================//
void massage_proc(void){
	if(smart_bed_display.status != MODE_MASSAGE)
	{
		smart_bed_display.status = MODE_MASSAGE;	// 마사지 display 갱신
		smart_bed_display.display_refresh = true;// 마사지 display
		memset(&cursor, 0, sizeof(CURSOR));// key init
		cursor.set_max = 2;
		// [전시모드] 수면(offset 6) 제외 → 6개(파도/지압/추나/스트레칭/롤링/시소)만 노출
	    cursor.type_max = bed_status.reserved ? 6 : MASSAGE_UI_MAX;

		// 동작 중인 마사지가 있으면 해당 마사지로 커서 이동 및 동작 상태 복원
		// running_massage_type은 CMD offset(0~11), UI index로 역매핑 필요
		if(running_massage_type >= 0 && running_massage_type < MASSAGE_MAX &&
		   running_massage_type != 11 &&	// 트위스트(0x3C = offset 11) 스킵
		   bed_status.run_state != 0){
			int i;
			for(i = 0; i < cursor.type_max; i++) {
				if(massage_cmd_offset[i] == running_massage_type) {
					cursor.type = i;
					break;
				}
			}
			if(bed_status.run_state == 2)
				conform_key_run = CMD3_RESTART;
			else
				conform_key_run = CMD3_PAUSE;
		} else {
			conform_key_run = CMD3_START;
			running_massage_type = -1;
		}

		memcpy(&temp_body, &massage[massage_cmd_offset[cursor.type]], sizeof(BODY));// 현재 설정 임시 버퍼로 복사 (offset 기준)

		remocon_key.key_val = 0xFF;
		body_set_max[0] = 0xC0;// time
		body_set_max[1] = 0x60;	// high
		massage_config_mode  = 0;	// 선택 모드로 시작
		massage_config_anim  = 0;
		massage_setting_focus = 0;
		massage_wave_frame   = 0;	// 화면 진입 시 애니메이션 처음부터
		return;
	}

	// ===== 설정 모드 (일반) : 위/아래=항목 이동, 좌/우=값 변경, 확인=시작, 마사지키=뒤로 =====
	if(!bed_status.reserved && massage_config_mode){
		int* pv = (massage_setting_focus == 0) ? &massage_intensity
		        : (massage_setting_focus == 1) ? &massage_speed : &massage_repeat;
		switch(remocon_key.key_val){
			case MASSAGE_KEY:	// 뒤로 → 선택 모드
			case SET_KEY:		// 설정창 열린 상태에서 SET도 내려감(뒤로)
				massage_config_mode = 0;
				smart_bed_display.display_refresh = true;
				break;
			case UP_KEY:
				massage_setting_focus = (massage_setting_focus + 2) % 3;
				smart_bed_display.display_refresh = true;
				break;
			case DOWN_KEY:
				massage_setting_focus = (massage_setting_focus + 1) % 3;
				smart_bed_display.display_refresh = true;
				break;
			case LEFT_KEY:		// 값 감소
				if(*pv > 0) (*pv)--;
				smart_bed_display.display_refresh = true;
				break;
			case RIGHT_KEY:		// 값 증가
				if(*pv < 2) (*pv)++;
				smart_bed_display.display_refresh = true;
				break;
			case CONFORM_KEY:	// 시작
				conform_key_proc(CMD2_MASSAGE + massage_cmd_offset[cursor.type]);
				break;
			default:
				break;
		}
		remocon_key.key_val = 0xFF;
		return;
	}

	switch(remocon_key.key_val){
		case MASSAGE_KEY:	// 마사지 버튼(MASSAGE_KEY 물리위치) 재입력 → 홈
			smart_bed_status.status = MODE_HOME;
			remocon_key.key_val = 0xFF;
			break;
		case UP_KEY:	// 위 → 다음 모드(정상 순서 진행). 전시: 아랫줄↔윗줄
			if(bed_status.reserved){		// 3열×2행 격자
				int col = cursor.type % 3, row = cursor.type / 3;
				row = (row + 1) % 2;		// 2행이라 위/아래 동일 토글
				cursor.type = row * 3 + col;
			} else {
				cursor.type++;
				if(cursor.type >= cursor.type_max)
					cursor.type = 0;
			}
			memcpy(&temp_body, &massage[massage_cmd_offset[cursor.type]], sizeof(BODY));
			massage_wave_frame = 0;	// 모드 전환 시 애니메이션 처음부터
			conform_key_run = 0;
			smart_bed_display.display_refresh = true;
			remocon_key.key_val = 0xFF;
			break;
		case DOWN_KEY:	// 아래 → 이전 모드. 전시: 윗줄↔아랫줄
			if(bed_status.reserved){		// 3열×2행 격자
				int col = cursor.type % 3, row = cursor.type / 3;
				row = (row + 1) % 2;
				cursor.type = row * 3 + col;
			} else {
				if(cursor.type)
					cursor.type--;
				else
					cursor.type = cursor.type_max - 1;
			}
			memcpy(&temp_body, &massage[massage_cmd_offset[cursor.type]], sizeof(BODY));
			massage_wave_frame = 0;	// 모드 전환 시 애니메이션 처음부터
			conform_key_run = 0;
			smart_bed_display.display_refresh = true;
			remocon_key.key_val = 0xFF;
			break;
		case LEFT_KEY:	// 전시: 6칸 전체 연속 순환(이전) — 1층1칸에서 좌 → 마지막칸 (일반: 미사용)
			if(bed_status.reserved){
				cursor.type = (cursor.type + cursor.type_max - 1) % cursor.type_max;
				memcpy(&temp_body, &massage[massage_cmd_offset[cursor.type]], sizeof(BODY));
				massage_wave_frame = 0;	// 모드 전환 시 애니메이션 처음부터
				conform_key_run = 0;
				smart_bed_display.display_refresh = true;
			}
			remocon_key.key_val = 0xFF;
			break;
		case RIGHT_KEY:	// 전시: 6칸 전체 연속 순환(다음) — 1층3칸 → 2층1칸, 마지막칸 → 1층1칸 (일반: 미사용)
			if(bed_status.reserved){
				cursor.type = (cursor.type + 1) % cursor.type_max;
				memcpy(&temp_body, &massage[massage_cmd_offset[cursor.type]], sizeof(BODY));
				massage_wave_frame = 0;	// 모드 전환 시 애니메이션 처음부터
				conform_key_run = 0;
				smart_bed_display.display_refresh = true;
			}
			remocon_key.key_val = 0xFF;
			break;
		case CONFORM_KEY:
			// 새로 시작(START)이면 설정 모드로 진입(전환 애니메이션). 실행 중이면 기존대로 일시정지/재시작.
			if(!bed_status.reserved && conform_key_run == CMD3_START){
				massage_config_mode = 1;
				smart_bed_display.display_refresh = true;
			} else {
				// UI cursor 인덱스를 실제 CMD offset으로 매핑 (트위스트 스킵)
				conform_key_proc(CMD2_MASSAGE + massage_cmd_offset[cursor.type]);
			}
			remocon_key.key_val = 0xFF;
			break;
		case SET_KEY:	// 설정 버튼 → 확인과 동일하게 설정 모드 진입(슬라이드업)
			if(!bed_status.reserved)
				massage_config_mode = 1;
			smart_bed_display.display_refresh = true;
			remocon_key.key_val = 0xFF;
			break;
		default:
			remocon_key.key_val = 0xFF;
	}
}


// UTF-8 문자 수 (한글=3바이트 등 멀티바이트 1글자로 카운트)
static int utf8_char_count(const char* s){
	int n = 0;
	while(*s){
		U8 c = (U8)*s;
		int len = (c < 0x80) ? 1 : ((c & 0xE0) == 0xC0) ? 2 : ((c & 0xF0) == 0xE0) ? 3 : 1;
		s += len; n++;
	}
	return n;
}

// UTF-8 문자열을 앞 at글자 / 나머지 로 분할 (a,b 버퍼 필요)
static void utf8_split(const char* s, int at, char* a, char* b){
	int n = 0; const char* p = s;
	while(*p && n < at){
		U8 c = (U8)*p;
		int len = (c < 0x80) ? 1 : ((c & 0xE0) == 0xC0) ? 2 : ((c & 0xF0) == 0xE0) ? 3 : 1;
		while(len-- > 0 && *p) *a++ = *p++;
		n++;
	}
	*a = 0;
	while(*p) *b++ = *p++;
	*b = 0;
}

// [전시모드 공용] 교대부양 일반 박스 규격(88×135, 라운드10, 28pt)으로 라벨 박스 1개 그림.
//   이름이 4글자 이상이면 2글자씩 2줄로 줄바꿈해 박스 안에 맞춤.
static void draw_care_grid_box(int bx, int by, int bw, int bh, const char* nm, bool sel){
	U32 GSEL = MAKE_COLORREF(210, 180, 120);	// 교대부양 일반 박스색 (선택)
	U32 GUN  = MAKE_COLORREF( 90,  85,  70);	// (비선택)
	U32 GBD  = MAKE_COLORREF(255, 255, 255);
	int nc = utf8_char_count(nm);
	int full_w = estimate_text_width_28(nm);
	draw_roundrectfill(bx, by, bw, bh, 10, sel ? GSEL : GUN);
	if(sel) draw_roundrect(bx, by, bw, bh, 10, GBD);
	egl_font_set_color(g_pFontKor, sel ? MAKE_COLORREF(30, 30, 30) : MAKE_COLORREF(225, 225, 225));
	if(nc >= 4 && full_w > bw - 12){		// 박스 폭을 넘칠 때만 2줄로 (좁은 박스 전용)
		char a[24], b[24];
		int tw1, tw2, line_gap = 34, y0;
		utf8_split(nm, nc / 2, a, b);		// 4글자 → 2글자 + 2글자
		tw1 = estimate_text_width_28(a);
		tw2 = estimate_text_width_28(b);
		y0 = by + (bh - (28 + line_gap)) / 2 + 4;
		draw_text_kr(g_pFontKor, bx + (bw - tw1) / 2, y0, a);
		draw_text_kr(g_pFontKor, bx + (bw - tw2) / 2, y0 + line_gap, b);
	} else {
		int tw = estimate_text_width_28(nm);
		draw_text_kr(g_pFontKor, bx + (bw - tw) / 2, by + (bh - 28) / 2, nm);
	}
}

// 대각선(파라메트릭) 두께 선 — 선 프리미티브가 없어 작은 사각형을 이어 그림
static void draw_diag_line(int x0, int y0, int x1, int y1, int thick, U32 color){
	int dx = x1 - x0, dy = y1 - y0;
	int adx = dx < 0 ? -dx : dx;
	int ady = dy < 0 ? -dy : dy;
	int steps = (adx > ady) ? adx : ady;
	int i;
	if(steps == 0){ draw_rectfill(x0, y0, thick, thick, color); return; }
	for(i = 0; i <= steps; i++){
		draw_rectfill(x0 + dx * i / steps, y0 + dy * i / steps, thick, thick, color);
	}
}

// 그리드 위 사람 실루엣 — 이미지(person.suf, 외곽선만·내부 투명)를 그리드 중앙에 오버레이.
//   투명 부분은 그리드가 비침. 그리드 세로 105~433(중앙 269), 가로 중앙 160.
static void draw_human_silhouette(void)
{
	if(!person_img) return;
	draw_surface(person_img, 160 - person_img->w / 2, 269 - person_img->h / 2);
}

// 비트폰트 폭 (한글=kw, ASCII=kw/2)
static int kr_bitfont_width(const char* s, int kw){
	int w = 0; const char* p = s;
	while(*p){ U8 c = (U8)*p;
		if(c < 0x80)                { w += kw / 2; p += 1; }
		else if((c & 0xE0) == 0xC0) { w += kw;     p += 2; }
		else if((c & 0xF0) == 0xE0) { w += kw;     p += 3; }
		else                        { p += 1; } }
	return w;
}

// 동작상태 텍스트 — 공간(박스 높이)을 최대한 채우게 크게: 숫자=g_pFont40(40px), 그 외(한글/공백)=g_pFontKor24(24px).
//   문자열을 숫자런/비숫자런으로 나눠 각각 그리고 x를 진행. 세로: 숫자 가운데, 한글은 +12(하단 정렬).
static void draw_status_text_mixed(int x, int by, int bh, const char* s)
{
	int y_num = by + (bh - 40) / 2;		// 숫자 40px 세로 가운데
	int y_kr  = by + (bh - 24) / 2;		// 한글 24px 세로 가운데 (숫자와 같은 중심)
	const char* p = s;
	char seg[64];
	while(*p){
		int n = 0;
		if(*p >= '0' && *p <= '9'){		// 숫자 런
			while(*p >= '0' && *p <= '9' && n < 63) seg[n++] = *p++;
			seg[n] = 0;
			egl_font_set_color(g_pFont40, MAKE_COLORREF(255, 255, 255));
			bmpfont_draw(g_pFont40, x, y_num, seg);
			x += (int)text_width(g_pFont40, seg);
		} else {						// 비숫자 런(한글/공백 등)
			while(*p && !(*p >= '0' && *p <= '9') && n < 63) seg[n++] = *p++;
			seg[n] = 0;
			egl_font_set_color(g_pFontKor24, MAKE_COLORREF(255, 255, 255));
			draw_text_kr(g_pFontKor24, x, y_kr, seg);
			x += kr_bitfont_width(seg, 24);
		}
	}
}

// 상단 상태 헤더 — 좌: 모드상태 텍스트(아이콘 없음, 4글자), 우: 온도·음량·무중력 pill (전 화면 공통)
// 동작 상태 내용(재생/정지 아이콘 + 모드 상태 텍스트)을 주어진 박스(bx,by,bw,bh)에 그림.
//   use_scroll=true: 좁은 박스에서 긴 텍스트 마퀴 스크롤. false: 넓은 박스(스크롤 불필요).
static void draw_status_content(int bx, int by, int bw, int bh, bool use_scroll)
{
	int icon_x = bx + 7;
	int icon_y = by + (bh - 18) / 2;
	// 아이콘: 종료중/전환홈 → ▶❚ / 동작중(state 1·3) → ‖ / 일시정지(2) → ▶ / 대기 → ▶❚
	if(stdby_in_progress || external_stopping ||
	   (init_home_pending && mode_switch_from_running))
		draw_play_icon(icon_x, icon_y, 18, MAKE_COLORREF(255,  80,  80));
	else if(bed_status.current_mode != 0 &&
	        (bed_status.run_state == 1 || bed_status.run_state == 3))
		draw_pause_icon(icon_x, icon_y, 18, MAKE_COLORREF( 80, 220, 100));
	else if(bed_status.current_mode != 0 && bed_status.run_state == 2)
		draw_triangle_icon(icon_x, icon_y, 18, MAKE_COLORREF(255, 200,  50));
	else
		draw_play_icon(icon_x, icon_y, 18, MAKE_COLORREF(255,  80,  80));

	{
		const char* stxt = get_mode_status_text(bed_status.current_mode, bed_status.run_state);
		// 넓은 박스(HOME)이고, 실제 모드 표시 중(대기/종료중/초기화중 제외)이면 우측에 경과시간
		bool show_elapsed = (bw >= 200)
			&& strcmp(stxt, "모드를 선택해 주세요")
			&& strcmp(stxt, "종료중")
			&& strcmp(stxt, "초기화 중");
		int elap_w = show_elapsed ? 120 : 0;

		int text_x = icon_x + 22;	// 아이콘 뒤
		CLIP_RECT nc, oc;
		nc.x = text_x;
		nc.y = by;
		nc.endx = bx + bw - 3 - elap_w;		// 경과시간 자리만큼 좁힘
		nc.endy = by + bh;
		oc = draw_set_clip_window(getbackframe(), &nc);
		int off = use_scroll ? (int)mode_text_offset : 0;
		draw_status_text_mixed(text_x - off, by, bh, stxt);	// 숫자=g_pFont28(28), 한글=g_pFontKor16(16) — 경과시간과 동일
		draw_set_clip_window(getbackframe(), &oc);

		if(show_elapsed){
			// 모드 텍스트와 경과시간 사이 세로 구분선
			draw_rectfill(bx + bw - elap_w, by + 8, 1, bh - 16, MAKE_COLORREF(120, 125, 135));
			draw_time_content_compact(bx + bw - elap_w, by, elap_w, bh);
		}
	}
}

// 경과시간 — 아이콘 없이 박스 전체를 써서 가운데 정렬.
//   "00분00초" 기본. 시간이 차면 상위 단위(시간/일/년)까지 붙여 표시.
// 폰트별 텍스트 폭 (경과시간 세그먼트 정렬용).
static int time_seg_w(EGL_FONT* f, const char* s){
	return (f == g_pFontKor16) ? kr_text_width_16(s) : (int)estimate_text_width_28(s);
}
// 경과시간 한 줄: (숫자[numFont] + 단위[unitFont]) 세그먼트를 가로 가운데 정렬해 그림.
//   단위 폰트/색을 작고 흐리게 주면 "숫자 크게, 단위 작게" 표현.
static void draw_time_units_line(int bx, int bw, EGL_FONT* numFont, int numY,
                                 EGL_FONT* unitFont, int unitY,
                                 const int* vals, const char** units, int n)
{
	const int SEG_GAP = 6;	// 세그먼트(값+단위) 사이 간격 (예: "1일 0시간")
	char nb[8];
	int i, total = 0, x;
	for(i = 0; i < n; i++){
		sprintf(nb, "%d", vals[i]);
		total += time_seg_w(numFont, nb) + time_seg_w(unitFont, units[i]);
	}
	if(n > 1) total += SEG_GAP * (n - 1);
	x = bx + (bw - total) / 2;
	for(i = 0; i < n; i++){
		sprintf(nb, "%d", vals[i]);
		egl_font_set_color(numFont, MAKE_COLORREF(255, 255, 255));
		draw_text_kr(numFont, x, numY, nb);
		x += time_seg_w(numFont, nb);
		egl_font_set_color(unitFont, MAKE_COLORREF(170, 175, 185));	// 단위는 흐리게(작게 보이도록)
		draw_text_kr(unitFont, x, unitY, units[i]);
		x += time_seg_w(unitFont, units[i]);
		if(i < n - 1) x += SEG_GAP;	// 세그먼트 사이 공백
	}
}
// 경과시간 표시 (아이콘 없이 박스 안에 가운데 정렬) — 메인보드 값(bed_status.elapsed_sec).
//   1시간 미만: 한 줄 "MM분SS초" (숫자 28 + 단위 16 작게).
//   1시간 이상: 두 줄 — 윗줄 (년)일시간 / 아랫줄 분초 (모두 16, 작게) → 박스 밖으로 안 나감.
static void draw_time_content_compact(int bx, int by, int bw, int bh)
{
	U32 total_sec = bed_status.elapsed_sec;	// 메인보드 계산값 → 연결 끊겼다 붙어도 유지
	U32 sec = total_sec % 60;
	U32 min = (total_sec / 60) % 60;
	U32 hr  = (total_sec / 3600) % 24;
	U32 day = (total_sec / 86400UL) % 365;
	U32 yr  = total_sec / (86400UL * 365UL);
	if(yr == 0 && day == 0 && hr == 0){
		// 1시간 미만 — 한 줄. 숫자는 온열/음량과 100% 동일(g_pFont28 + bmpfont_draw, y=11),
		//   단위(분/초)는 작게(g_pFontKor16). 가로 가운데 정렬.
		char nb[8];
		int seg_v[2]; const char* seg_u[2] = { "분", "초" };
		int i, wtot = 0, x;
		int ny = by + (bh - 28) / 2;		// 숫자 28px 세로 가운데
		seg_v[0] = (int)min; seg_v[1] = (int)sec;
		for(i = 0; i < 2; i++){
			sprintf(nb, "%02d", seg_v[i]);
			wtot += (int)text_width(g_pFont28, nb) + kr_text_width_16(seg_u[i]);
		}
		x = bx + (bw - wtot) / 2;			// 가로 가운데
		for(i = 0; i < 2; i++){
			sprintf(nb, "%02d", seg_v[i]);
			egl_font_set_color(g_pFont28, MAKE_COLORREF(255, 255, 255));
			bmpfont_draw(g_pFont28, x, ny, nb);
			x += (int)text_width(g_pFont28, nb);
			egl_font_set_color(g_pFontKor16, MAKE_COLORREF(170, 175, 185));
			draw_text_kr(g_pFontKor16, x, ny + 8, seg_u[i]);
			x += kr_text_width_16(seg_u[i]);
		}
	} else {
		// 1시간 이상 — 두 줄 (윗줄: (년)일시간 / 아랫줄: 분초)
		int v1[3]; const char* u1[3]; int n1 = 0;
		int v2[2]; const char* u2[2] = { "분", "초" };
		if(yr > 0){ v1[n1] = (int)yr; u1[n1] = "년"; n1++; }
		if(yr > 0 || day > 0){ v1[n1] = (int)day; u1[n1] = "일"; n1++; }
		v1[n1] = (int)hr; u1[n1] = "시간"; n1++;
		v2[0] = (int)min; v2[1] = (int)sec;
		draw_time_units_line(bx, bw, g_pFontKor16, by + 6,  g_pFontKor16, by + 6,  v1, u1, n1);
		draw_time_units_line(bx, bw, g_pFontKor16, by + 27, g_pFontKor16, by + 27, v2, u2, 2);
	}
}

// 온열 물결 — 길게 편 S(세로 사인 한 주기) 곡선을 n개(단계). 다점 연결로 부드럽게.
static void draw_heat_waves_icon(int x, int y, int n, U32 color)
{
	static const int off[9] = {0, 71, 100, 71, 0, -71, -100, -71, 0};	// sin*100 (한 주기=S)
	const int H = 26, amp = 3, gapx = 9, PTS = 9;
	int c, i;
	if(n > 3) n = 3;
	for(c = 0; c < n; c++){
		int cx = x + c * gapx + amp;
		int px = cx + (amp * off[0]) / 100, py = y;
		for(i = 1; i < PTS; i++){
			int nx = cx + (amp * off[i]) / 100;
			int ny = y + (H * i) / (PTS - 1);
			draw_diag_line(px, py, nx, ny, 2, color);
			px = nx; py = ny;
		}
	}
}

// 정수 제곱근 (작은 값용)
static int isqrt_small(int v){
	int r = 0;
	while((r + 1) * (r + 1) <= v) r++;
	return r;
}

// 소리 세기 — 스피커(cx,cy) 중심 동심 원호 ")" 를 n개.
//   원 방정식 x=√(R²−dy²)로 y를 대칭 스캔 → 위/아래 완전 대칭. 반경을 키워 바깥으로.
static void draw_sound_arcs(int cx, int cy, int n, U32 color)
{
	int k, dy;
	if(n > 3) n = 3;
	for(k = 0; k < n; k++){
		int R = 9 + k * 4;			// 반경 (안→밖)
		int A = (R * 81) / 100;		// 호 반높이(약 54°)
		for(dy = -A; dy <= A; dy++){
			int xoff = isqrt_small(R * R - dy * dy);
			draw_rectfill(cx + xoff, cy + dy, 2, 1, color);
		}
	}
}

// 우측 클러스터(온도/음량/무중력) — 헤더 공통.
//   좌측 박스(5~157, 폭 152)와 동일 크기의 우측 절반 박스(163~315, 폭 152)에 3 pill 균등 배치.
static void draw_status_header_cluster(int bx0, bool frame)
{
	const int BX = bx0, BW = 152;			// 시작 x, 폭 152
	const int PAD = 4, GAP = 3;
	const int PW = (BW - 2 * PAD - 2 * GAP) / 3;	// (152-8-6)/3 = 46
	int p0 = BX + PAD;						// 온도
	int p1 = p0 + PW + GAP;					// 음량
	int p2 = p1 + PW + GAP;					// 무중력

	if(frame)
		draw_roundrect(BX, 0, BW, 50, 8, MAKE_COLORREF(220, 50, 50));

	U32 SIL = MAKE_COLORREF(78, 82, 92);	// 꺼짐 실루엣(흐린 회색)

	// 온열 — 켜짐: 주황 물결 × 단계 / 꺼짐: 흐린 실루엣(물결 3개)
	if(bed_status.heat_level > 0)
		draw_heat_waves_icon(p0 + 6, 12, bed_status.heat_level, MAKE_COLORREF(255, 140, 30));
	else
		draw_heat_waves_icon(p0 + 6, 12, 3, SIL);

	// 음량 — 켜짐: 초록 스피커 + 호 × 단계 / 꺼짐: 흐린 스피커+호3 실루엣 (스피커 단독 상태 없음)
	if(bed_status.volume_level > 0){
		U32 sc = MAKE_COLORREF(80, 210, 90);
		draw_speaker_icon(p1 + 2, 12, 26, sc);
		draw_sound_arcs(p1 + 15, 25, bed_status.volume_level, sc);
	} else {
		draw_speaker_icon(p1 + 2, 12, 26, SIL);
		draw_sound_arcs(p1 + 15, 25, 3, SIL);
	}

	// 무중력 — 2줄 ZERO / G (줄 간격 좁힘). 켜짐=흰 / 꺼짐=흐린 실루엣. 사선 없음.
	{
		int tw1 = kr_text_width_16("ZERO");
		int tw2 = kr_text_width_16("G");
		int zx = (bx0 <= 5) ? 270 : p2;		// HOME은 ZERO/G를 우측 상단으로 분리
		U32 tc = bed_status.zero_gravity ? MAKE_COLORREF(255, 255, 255) : SIL;
		egl_font_set_color(g_pFontKor16, tc);
		draw_text_kr(g_pFontKor16, zx + (PW - tw1) / 2, 11, "ZERO");
		draw_text_kr(g_pFontKor16, zx + (PW - tw2) / 2, 25, "G");
	}
}

// 상단 상태 헤더 (전 화면 공통): 좌측 동작 상태(좁음, 마퀴) + 우측 온도/음량/무중력.
//   ※ 홈 화면(draw_status_overlay)은 박스 자리 바꿈으로 좌측에 경과시간을 넣고
//     동작 상태는 아래 넓은 박스에 직접 그리므로 이 함수를 쓰지 않는다.
static void draw_status_header(void){
	// HOME 상단 1행과 동일: 아이콘 클러스터만(온열/사운드 좌, ZERO G 우), 박스·테두리 없음.
	draw_status_header_cluster(5, false);
}

// 화면 상단 2번째 줄 — HOME과 동일: 위/아래 가로 구분선 + 동작상태(아이콘+텍스트+우측 경과시간).
//   (기존 제목바 자리를 대체하므로 제목 문자열 s는 사용하지 않는다.)
static void draw_title_bar(const char* s){
	(void)s;
	draw_rectfill(5, 55, 310, 2, MAKE_COLORREF(90, 95, 105));	// 위 구분선
	draw_rectfill(5, 99, 310, 2, MAKE_COLORREF(90, 95, 105));	// 아래 구분선
	draw_status_content(5, 55, 310, 44, false);					// 동작 상태(경과시간 우측)
}

// 홈 안내 푸터 — 16pt, 회색, 가로 중앙 정렬, y 고정 (전 화면 통일)
//   16pt 비트폰트 실제 폭(한글/전각 16px, ASCII 8px)으로 계산해 정확히 중앙 배치.
// draw_triangle_up/down은 posture 섹션에 정의 — 여기서 쓰므로 forward 선언
static void draw_triangle_up(int cx, int cy, int size, U32 color);
static void draw_triangle_down(int cx, int cy, int size, U32 color);

// 하단 푸터 — 욕창케어와 동일: 이동 ⇅ / 시작 [확인] / 홈으로 [화면명].
//   구분선(440)~바닥(480) 세로 중앙(460) 정렬. name = 화면명(그 화면 버튼 라벨).
static void draw_home_footer(SURFACE* tag_img){
	int fcy = 460, fty = fcy - 8;
	draw_rectfill(5, 440, 310, 1, MAKE_COLORREF(62, 66, 76));	// 하단 구분선
	egl_font_set_color(g_pFontKor16, MAKE_COLORREF(205, 210, 220));
	draw_text_kr(g_pFontKor16, 12, fty, "이동");
	{	// ⇅ 아이콘 — 작게(9) + 위아래 간격(6), 세로 중앙
		U32 ac = MAKE_COLORREF(205, 210, 220);
		int as = 9, agap = 6, atop = fcy - (as * 2 + agap) / 2;
		draw_triangle_up(60, atop, as, ac);
		draw_triangle_down(60, atop + as + agap, as, ac);
	}
	{	// 시작 + 확인 이미지 (세로 중앙)
		int sx = 116;
		draw_text_kr(g_pFontKor16, sx, fty, "시작");
		if(confirm_btn_img){
			int cx = sx + kr_text_width_16("시작") + 5;
			draw_surface(confirm_btn_img, cx, fcy - confirm_btn_img->h / 2);
		}
	}
	{	// 홈으로(힌트) + 화면명 태그 이미지 — 우측(세로 중앙)
		int hw = kr_text_width_16("홈으로");
		int ix = tag_img ? (315 - tag_img->w) : 315;	// 태그 없으면 우측 끝
		if(tag_img) draw_surface(tag_img, ix, fcy - tag_img->h / 2);
		egl_font_set_color(g_pFontKor16, MAKE_COLORREF(205, 210, 220));
		draw_text_kr(g_pFontKor16, ix - 12 - hw, fty, "홈으로");
	}
}

// 마사지 전용 푸터 — 선택 모드: 이동/선택/홈으로,  설정 모드: 이동/시작/이전 화면
static void draw_massage_footer(SURFACE* tag_img, int config_mode){
	int fcy = 460, fty = fcy - 8;
	const char* center = config_mode ? "시작" : "선택";
	const char* right  = config_mode ? "이전" : "홈으로";
	draw_rectfill(5, 440, 310, 1, MAKE_COLORREF(62, 66, 76));	// 하단 구분선
	egl_font_set_color(g_pFontKor16, MAKE_COLORREF(205, 210, 220));
	draw_text_kr(g_pFontKor16, 12, fty, "이동");
	{	// ⇅ 아이콘 (세로 중앙)
		U32 ac = MAKE_COLORREF(205, 210, 220);
		int as = 9, agap = 6, atop = fcy - (as * 2 + agap) / 2;
		draw_triangle_up(60, atop, as, ac);
		draw_triangle_down(60, atop + as + agap, as, ac);
	}
	{	// 선택/시작 + 확인 이미지 (세로 중앙)
		int sx = 116;
		egl_font_set_color(g_pFontKor16, MAKE_COLORREF(205, 210, 220));
		draw_text_kr(g_pFontKor16, sx, fty, center);
		if(confirm_btn_img){
			int cx = sx + kr_text_width_16(center) + 5;
			draw_surface(confirm_btn_img, cx, fcy - confirm_btn_img->h / 2);
		}
	}
	{	// 홈으로/이전 화면 + 마사지 태그 이미지 — 우측(세로 중앙)
		int hw = kr_text_width_16(right);
		int ix = tag_img ? (315 - tag_img->w) : 315;
		if(tag_img) draw_surface(tag_img, ix, fcy - tag_img->h / 2);
		egl_font_set_color(g_pFontKor16, MAKE_COLORREF(205, 210, 220));
		draw_text_kr(g_pFontKor16, ix - 12 - hw, fty, right);
	}
}

// 마사지 설정 행 — 라벨(좌) + 3택 옵션(우). focused=현재 포커스 행(밝게), sel=선택 값(강조 박스).
static void draw_massage_setting_row(int y, const char* label, const char** opts, int sel, int focused){
	static const int OX[3] = {120, 195, 268};	// 옵션 중심 x
	int i;
	// 라벨 (포커스 행은 흰색, 비포커스는 흐리게)
	egl_font_set_color(g_pFontKor16, focused ? MAKE_COLORREF(255, 255, 255) : MAKE_COLORREF(150, 155, 165));
	draw_text_kr(g_pFontKor16, 15, y, label);
	for(i = 0; i < 3; i++){
		int w = kr_text_width_16(opts[i]);
		int x = OX[i] - w / 2;
		if(i == sel){
			draw_roundrectfill(x - 8, y - 4, w + 16, 24, 6,
				focused ? MAKE_COLORREF(90, 220, 90) : MAKE_COLORREF(70, 120, 80));
			egl_font_set_color(g_pFontKor16, MAKE_COLORREF(20, 25, 38));
		} else {
			egl_font_set_color(g_pFontKor16, MAKE_COLORREF(120, 125, 135));
		}
		draw_text_kr(g_pFontKor16, x, y, opts[i]);
	}
}

// PDF 슬라이드 10 — 마사지 (12 모드, 3-항목 스크롤 윈도우)
void massage_draw(void){
	char buf[8];
	int slot;
	U32 BORDER = MAKE_COLORREF(220, 50, 50);
	U32 icon_color, pill_bg;

	set_draw_target(getbackframe());
	draw_rectfill(0, 0, 320, 480, MAKE_COLORREF(20, 25, 38));

	// 헤더 (공용 — 온도/음량/무중력)
	draw_status_header();

	// 마사지 타이틀 (욕창케어와 동일)
	draw_title_bar("마사지");

	// 카드 슬라이드 — 외형은 "현재 위치(슬롯)"에 따라 결정
	//   middle slot 가장 가까운 카드 = 노란 큰 박스 (포커싱)
	//   나머지 = 회색 작은 박스
	//   카드들이 슬라이드하면서 yellow가 자연스럽게 이전→현재로 넘겨받음
	if(bed_status.reserved){
		// ===== [전시모드] 마사지 = 3열×2행 그리드 (애니메이션 없음) =====
		//   수면 제외 6개: 파도타기/지압/추나 (윗줄) · 스트레칭/롤링/시소 (아랫줄)
		//   박스 규격은 교대부양 일반 박스와 동일: 88×135, 라운드 10, 동일 색, 28pt
		const int COLS = 3;
		int n = (int)cursor.type_max;              // 전시모드 6
		int gap = 14;                              // 옆·아래 간격 통일
		int mx = 5;                                // 타이틀/타 요소와 동일한 좌우 여백 (5~315)
		int box_h = 135;
		int box_w = (320 - 2 * mx - (COLS - 1) * gap) / COLS;  // (320-10-28)/3 = 94
		int gx = gap, gy = gap;                    // 5+94+14+94+14+94+5 = 320
		// 세로 중앙: 타이틀 아래(99)~푸터(458) 사이. 블록 높이 = 2*135+14 = 284.
		//   area_top = 99 + ((458-99) - 284)/2 = 137 (위 38 / 아래 37)
		int area_top = 137;
		int i;
		for(i = 0; i < n; i++){
			int col = i % COLS;
			int row = i / COLS;
			int bx = mx + col * (box_w + gx);
			int by = area_top + row * (box_h + gy);
			bool s = (cursor.type == (U8)i);
			const char *nm = get_mode_name(0x30 + massage_cmd_offset[i]);
			draw_care_grid_box(bx, by, box_w, box_h, nm, s);	// 4글자는 2줄 자동
		}
	} else {
		// [일반] 2단 흐름 — p=전환 진행도(0:선택 ~ 100:설정, tick이 갱신).
		//   선택 모드 : 이름(상단) + 측면 매트리스(하단).  CONFORM → 설정 모드.
		//   설정 모드 : 측면 매트리스가 상단으로 상승 + 이름 사라짐 + 설정 3행이 밑에서 상승.
		const char* nm = get_mode_name(0x30 + massage_cmd_offset[cursor.type]);
		int p = massage_config_anim;

		// --- 측면 매트리스: 하단(rest_y 360) → 상단(160) 상승 ---
		{
			static const int SIN16[16] = {50,69,85,96,100,96,85,69,50,31,15,4,0,4,15,31};	// 0~100
			static const int PH[4]  = {2, 0, 8, 3};		// (일반 모드) 대표 패턴 (높이는 36 통일)
			// 파도타기 전용 웨이브 — 각 건반이 대칭 삼각파(상승=하강 같은 기울기, 연속=끊김없음).
			//   3스텝에 걸쳐 0→100 상승, 3스텝에 걸쳐 100→0 하강 → 하강 시 자동으로 1/3→2/3→완전.
			//   그룹별 정점 step(각 3스텝 간격): (1,2,7)=2 (3,8)=5 (4,9)=8 (5,10)=11 (6,11)=14.
			//   그룹5(14) 하강 = 그룹1(2+15) 상승 → step6→step1 자연스러운 루프.
			//                            k1 k2 k3 k4  k5  k6 k7 k8 k9 k10 k11
			static const int PEAK[11] = { 2, 2, 5, 8, 11, 14, 2, 5, 8, 11, 14};
			int SPB = 6, PERIODF = 90;	// step=8프레임(160ms), 사이클 15step(≈2.4s) — 1.5배 빠르게
			int is_wave = (massage_cmd_offset[cursor.type] == 0);	int is_chuna = (massage_cmd_offset[cursor.type] == 2);	int is_stretch = (massage_cmd_offset[cursor.type] == 3);	int is_roll = (massage_cmd_offset[cursor.type] == 4);	int is_seesaw = (massage_cmd_offset[cursor.type] == 5);	int is_jiap = (massage_cmd_offset[cursor.type] == 1);	// 파도타기 / 추나 / 스트레칭	// 파도타기(offset 0)
			int pat = cursor.type % 4;
			int phase = PH[pat], i;	// amp는 높이 통일(36)로 미사용
			int cw = 22, gap = 2, ch = 14;
			int sw = 11 * (cw + gap) - gap;				// 262
			int hx = 160 - sw / 2;						// 가운데(29)
			int rest_y = 360 - 200 * p / 100;			// 하단(360) → 상단(160)
			int fy = rest_y + ch + 4;
			draw_roundrectfill(hx, fy, sw, 8, 3, MAKE_COLORREF(55, 60, 70));	// 고정 바
			for(i = 0; i < 11; i++){
				int off;
				if(is_wave){
					int h;
					if(i == 0){
						// [1]번: 하강만 3단계 계단, 상승/정점/rest는 일반 삼각파와 '완전히 동일' 공식 → 7번과 똑같이 부드럽게 상승.
						int t = (massage_wave_frame - PEAK[0] * SPB) % PERIODF;
						if(t < 0) t += PERIODF;
						if(t > 0 && t < 9 * SPB){			// 하강 구간(9스텝 = 3단계)만 특수
							int p  = t / (3 * SPB);			// 단계 0,1,2
							int tp = t - p * (3 * SPB);		// 단계 내 프레임
							int top = 100 - p * 100 / 3;	// 단계 시작 높이(100,67,34)
							if(tp < SPB) h = top - tp * 100 / (3 * SPB);	// 1스텝에 1/3 하강(일반 속도)
							else         h = top - 100 / 3;					// 이후 유지
						} else {							// 상승·정점·rest = 일반 대칭 삼각파와 동일
							int dc = t;
							if(dc > PERIODF / 2) dc = PERIODF - dc;
							h = 100 - dc * 100 / (3 * SPB);
							if(h < 0) h = 0;
						}
					} else {
						// 그 외 건반: 최고점까지 원형거리로 대칭 삼각파(상승=하강 등속·연속).
						int d = (massage_wave_frame - PEAK[i] * SPB) % PERIODF;
						if(d < 0) d += PERIODF;
						if(d > PERIODF / 2) d = PERIODF - d;
						h = 100 - d * 100 / (3 * SPB);
						if(h < 0) h = 0;
					}
					off = 36 * h / 100;	// 높이 통일(추나 기준 36)
				} else if(is_chuna){
					// 추나: front(선발)↑/back(후발)↓ → 중간교차 → 반대(front↓/back↑) → 중간교차 → front '다시↑'.
					//   '다시↑' 교차(모든 셀이 중간서 만남)에서만 후발 1셀을 front로 넘겨 front를 {1}→{1..6}까지 성장 후 반복.
					int BEAT = 9, NC = 24;					// 키프레임당 9프레임, 24키프레임(6왕복)≈4.3s. 셀 이동속도≈2px/frame
					int cc = (massage_wave_frame / BEAT) % NC;
					int nc = (cc + 1) % NC;
					int fr = massage_wave_frame % BEAT;
					// front up/back down → 중간교차 → 반대 → 중간교차에서 '다시 up'일 때만 후발1셀을 front로 전송(왕복당 +1).
					// 4키프레임/그룹: sub0=E_up, sub1=중간, sub2=E_down, sub3=중간(여기서 후발1셀 front로 전송). g=block+1.
					int g0 = cc / 4 + 1, s0 = cc & 3;
					int h0 = (s0 & 1) ? 50 : (s0 == 0 ? ((i < g0) ? 100 : 0) : ((i < g0) ? 0 : 100));
					int g1 = nc / 4 + 1, s1 = nc & 3;
					int h1 = (s1 & 1) ? 50 : (s1 == 0 ? ((i < g1) ? 100 : 0) : ((i < g1) ? 0 : 100));
					int h = h0 + (h1 - h0) * fr / BEAT;		// config 간 선형보간(교차 매끄럽게)
					off = 36 * h / 100;	// 높이 통일 36
				} else if(is_stretch){
					// 스트레칭: 3파트 순차 — P1 하단[4,5,6]봉우리×3, P2 [1,2,3]봉우리×3, P3 [8~11]딥×3. 값=높이%(0/50/100).
					static const U8 SK[32][11] = {
								{  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0},
								{  0,   0,   0,  50, 100,  50,   0,   0,   0,   0,   0},
								{  0,   0,   0,  50,  50,  50,   0,   0,   0,   0,   0},
								{  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0},
								{  0,   0,   0,  50, 100,  50,   0,   0,   0,   0,   0},
								{  0,   0,   0,  50,  50,  50,   0,   0,   0,   0,   0},
								{  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0},
								{  0,   0,   0,  50, 100,  50,   0,   0,   0,   0,   0},
								{  0,   0,   0,  50,  50,  50,   0,   0,   0,   0,   0},
								{  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0},
								{ 50, 100,  50,   0,   0,   0,   0,   0,   0,   0,   0},
								{ 50,  50,  50,   0,   0,   0,   0,   0,   0,   0,   0},
								{  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0},
								{ 50, 100,  50,   0,   0,   0,   0,   0,   0,   0,   0},
								{ 50,  50,  50,   0,   0,   0,   0,   0,   0,   0,   0},
								{  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0},
								{ 50, 100,  50,   0,   0,   0,   0,   0,   0,   0,   0},
								{ 50,  50,  50,   0,   0,   0,   0,   0,   0,   0,   0},
								{  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0},
								{  0,   0,   0,   0,   0,   0,   0, 100, 100, 100, 100},
								{  0,   0,   0,   0,   0,   0,   0,  50, 100,  50, 100},
								{  0,   0,   0,   0,   0,   0,   0, 100, 100, 100, 100},
								{  0,   0,   0,   0,   0,   0,   0, 100,  50, 100,  50},
								{  0,   0,   0,   0,   0,   0,   0, 100, 100, 100, 100},
								{  0,   0,   0,   0,   0,   0,   0,  50, 100,  50, 100},
								{  0,   0,   0,   0,   0,   0,   0, 100, 100, 100, 100},
								{  0,   0,   0,   0,   0,   0,   0, 100,  50, 100,  50},
								{  0,   0,   0,   0,   0,   0,   0, 100, 100, 100, 100},
								{  0,   0,   0,   0,   0,   0,   0,  50, 100,  50, 100},
								{  0,   0,   0,   0,   0,   0,   0, 100, 100, 100, 100},
								{  0,   0,   0,   0,   0,   0,   0, 100,  50, 100,  50},
								{  0,   0,   0,   0,   0,   0,   0, 100, 100, 100, 100}
							};
					int SB = 12, NS = 32;
					int kf = (massage_wave_frame / SB) % NS;
					int nk = (kf + 1) % NS;
					int fr = massage_wave_frame % SB;
					int h = SK[kf][i] + ((int)SK[nk][i] - (int)SK[kf][i]) * fr / SB;
					off = 36 * h / 100;
				} else if(is_roll){
					// 롤링(massage_5 참고): roll_mask 10스텝(각 2셀↑2셀↓) × 3회, 단일 높이(100=다른 모드와 통일). 상태 누적 시뮬. 값=높이%.
					static const U8 RK[16][11] = {
							{  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0},
							{  0, 100,   0,   0,   0,   0, 100,   0,   0,   0,   0},
							{  0, 100, 100,   0,   0,   0, 100, 100,   0,   0,   0},
							{  0,   0, 100, 100,   0,   0,   0, 100, 100,   0,   0},
							{  0,   0,   0, 100, 100,   0,   0,   0, 100, 100,   0},
							{  0,   0,   0,   0, 100, 100,   0,   0,   0, 100, 100},
							{  0, 100,   0,   0,   0, 100, 100,   0,   0,   0, 100},
							{  0, 100, 100,   0,   0,   0, 100, 100,   0,   0,   0},
							{  0,   0, 100, 100,   0,   0,   0, 100, 100,   0,   0},
							{  0,   0,   0, 100, 100,   0,   0,   0, 100, 100,   0},
							{  0,   0,   0,   0, 100, 100,   0,   0,   0, 100, 100},
							{  0, 100,   0,   0,   0, 100, 100,   0,   0,   0, 100},
							{  0, 100, 100,   0,   0,   0, 100, 100,   0,   0,   0},
							{  0,   0, 100, 100,   0,   0,   0, 100, 100,   0,   0},
							{  0,   0,   0, 100, 100,   0,   0,   0, 100, 100,   0},
							{  0,   0,   0,   0, 100, 100,   0,   0,   0, 100, 100}
						};
											int RB = 18, INTRO = 11, STEADY = 5;	// rest 롤인 후 정상 롤 5-상태 무한 반복(내림 없음)
						int idx = massage_wave_frame / RB, fr = massage_wave_frame % RB, kf, nk;
						if(idx < INTRO){ kf = idx; nk = idx + 1; }
						else { int sidx = (idx - INTRO) % STEADY; kf = INTRO + sidx; nk = INTRO + (sidx + 1) % STEADY; }
						int h = RK[kf][i] + ((int)RK[nk][i] - (int)RK[kf][i]) * fr / RB;
						off = 36 * h / 100;
				} else if(is_seesaw){
					// 시소(massage_6 참고): 머리쪽 램프 상승 A ↔ 발쪽 램프 상승 B 교대(피벗 idx5). 단일 높이 100.
					static const U8 SS[3][11] = {
						{  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0},
						{100,  80,  60,  40,  20,   0,   0,   0,   0,   0,   0},
						{  0,   0,   0,   0,   0,   0,  20,  40,  60,  80, 100}
					};
					int SB = 18, INTRO = 1, STEADY = 2;	// rest->A 롤인 후 A<->B 무한(2px/frame)
					int idx = massage_wave_frame / SB, fr = massage_wave_frame % SB, kf, nk;
					if(idx < INTRO){ kf = idx; nk = idx + 1; }
					else { int sidx = (idx - INTRO) % STEADY; kf = INTRO + sidx; nk = INTRO + (sidx + 1) % STEADY; }
					int h = SS[kf][i] + ((int)SS[nk][i] - (int)SS[kf][i]) * fr / SB;
					off = 36 * h / 100;
				} else if(is_jiap){
					// 지압(massage_2 LEVEL1 참고): 짝(idx k, k+5)이 순차적으로 눌렀다(100) 뗐다(0). 단일 높이. rest 포함 → 진입 시 rest 시작.
					static const U8 JK[10][11] = {
						{  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0},
						{  0, 100,   0,   0,   0,   0, 100,   0,   0,   0,   0},
						{  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0},
						{  0,   0, 100,   0,   0,   0,   0, 100,   0,   0,   0},
						{  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0},
						{  0,   0,   0, 100,   0,   0,   0,   0, 100,   0,   0},
						{  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0},
						{  0,   0,   0,   0, 100,   0,   0,   0,   0, 100,   0},
						{  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0},
						{  0,   0,   0,   0,   0, 100,   0,   0,   0,   0, 100}
					};
										int JB = 18, NJ = 10;
					int kf = (massage_wave_frame / JB) % NJ;
					int nk = (kf + 1) % NJ;
					int fr = massage_wave_frame % JB;
					int h = JK[kf][i] + ((int)JK[nk][i] - (int)JK[kf][i]) * fr / JB;
					off = 36 * h / 100;
				} else {
					// 일반 모드: 20ms 프레임 + SIN16 선형보간. 셀 이동속도≈2px/frame(높이 36, 다른 모드와 통일).
					int pos = massage_wave_frame * 4 + i * phase * 9;	// index당 2.25프레임(0.44 index/frame)
					int a = (pos / 9) & 15, b = (a + 1) & 15;
					int v = SIN16[a] + (SIN16[b] - SIN16[a]) * (pos % 9) / 9;
					off = 36 * v / 100;	// 높이 통일 36
				}
				draw_roundrectfill(hx + i * (cw + gap), rest_y - off, cw, ch, 3, MAKE_COLORREF(80, 200, 80));
			}
		}

		// --- 선택 모드: 이름 + 시간 (전환 시작하면 살짝 오르며 사라짐) ---
		if(p < 45){
			int tw = estimate_text_width_28(nm);
			int ny = 115 - p * 2 / 5;					// 115 → 97
			egl_font_set_color(g_pFontKor, MAKE_COLORREF(255, 255, 255));
			draw_text_kr(g_pFontKor, (320 - tw) / 2, ny, nm);
		}

		// --- 설정 모드: 시간(매트리스 밑) + 강도/스피드/반복 3행 (밑에서 상승) ---
		if(p > 10){
			static const char* INTEN[3] = {"약", "중", "강"};
			static const char* SPEED[3] = {"느리게", "보통", "빠르게"};
			static const char* REP[3]   = {"1회", "2회", "무한"};
			int soff = (100 - p) * 2;					// p=100 →0 (아래에서 상승)
			int r1 = 240 + soff, r2 = 295 + soff, r3 = 350 + soff;
			if(r1 < 435) draw_massage_setting_row(r1, "강도",   INTEN, massage_intensity, massage_setting_focus == 0);
			if(r2 < 435) draw_massage_setting_row(r2, "스피드", SPEED, massage_speed,     massage_setting_focus == 1);
			if(r3 < 435) draw_massage_setting_row(r3, "반복",   REP,   massage_repeat,    massage_setting_focus == 2);
		}
	}
	(void)slot;

	// 푸터 — 전시모드는 공용, 일반은 선택/설정 모드별 텍스트
	if(bed_status.reserved)
		draw_home_footer(massage_tag_img);
	else
		draw_massage_footer(massage_tag_img, massage_config_mode);

	flip();
}

void massage_draw_legacy(void){
	int i = 0, num = 0, digits = 0;
	char string[100] = {0, };

	set_draw_target(getbackframe());// back frame select
	draw_surface(btm1_img,0,0);	// 배경 이미지(의미 없음)
	draw_surface(massage_title_img, TITLE_X, TITLE_Y);	// title 이미지
	draw_surface(massage_main_icon_img, MAIN_ICON_X, MAIN_ICON_Y);	// 메인 아이콘

	// 마사지 선택 이미지 (가운데 정렬 180x60)
	draw_surface(massage_sub_icon_img[cursor.type], MASSAGE_SUB_X, MASSAGE_SUB_Y);
	// 좌우 선택 화살표 (40x40, 세로 중앙 정렬)
	if(left_select_img)
		draw_surface(left_select_img, 25, MASSAGE_SUB_Y + 10);
	if(right_select_img)
		draw_surface(right_select_img, 255, MASSAGE_SUB_Y + 10);

// 설정 모드 표시 바
	draw_mode_indicator();

// BAR draw
	for(i = 0; i < MAX_BAR; i++)
	{
		if( bar[i] )
			draw_surface(bar_img, BAR_X, (BAR_Y + BAR_I * i));
	}
// 인체 이미지 draw
	if(body_info == true)
		draw_surface(body_imag, BODY_X, BODY_Y);	// body image

// 머리, 상체부, 하체부, 다리
	for(i = 0; i < BODY_MAX; i++)
	{
		if(cursor.mode != CURSOR_MODE)
		{// 설정 모드: 커서 선택 표시
			if(cursor.body == i)
				draw_surface(radio_avail_selt_img,radio_xy[i][0], radio_xy[i][1]);// 선택됨
			else
			{
				if(temp_body.body[i][0] & 0x01)
					draw_surface(radio_avail_img,radio_xy[i][0], radio_xy[i][1]);// Enable
				else
					draw_surface(radio_dis_selt_img,radio_xy[i][0], radio_xy[i][1]);// Disable
			}
		}
		else
		{// 일반 모드: Enable/Disable 상태만 표시
			if(temp_body.body[i][0] & 0x01)
				draw_surface(radio_avail_img,radio_xy[i][0], radio_xy[i][1]);// Enable
			else
				draw_surface(radio_dis_selt_img,radio_xy[i][0], radio_xy[i][1]);// Disable
		}
	}
// 선택 부위 하이라이트 테두리
	draw_body_highlight();

// 설정 값 표시 : 시간설정, 높이, 속도
	egl_font_set_color(g_pFont28, MAKE_COLORREF(255,255,255));
	for(i = 0; i < cursor.set_max; i++)
	{
		if(cursor.mode == CURSOR_SET)
		{
			if(cursor.set == i)
				draw_surface(dip_en_num_img, num2_xy[i][0], num2_xy[i][1]);// Enable 활성
			else
				draw_surface(dip_ds_num_img, num2_xy[i][0], num2_xy[i][1]);// Disable 비활성
		}
		else
		{
			draw_surface(dip_ds_num_img, num2_xy[i ][0], num2_xy[i ][1]);// Disable 비활성
		}
		debugprintf("\n\r -> 0x%x", temp_body.body[cursor.body][i+1]);
		if(i == 0) num = (temp_body.body[cursor.body][i+1]>>4) * 10;
		else
		{
			num = (temp_body.body[cursor.body][i+1]>>4);
			num *= 30;
			if(num > 160) num = 170;
		}
		
		sprintf(string, "%d", num);
		// bmpfont_draw(g_pFont28, num2_xy[i ][0]+15, num2_xy[i ][1]-3, string);
		digits = count_digits_string(num);
		switch(digits)
		{
			case 1:
				bmpfont_draw(g_pFont28, num2_xy[i ][0]+ 22, num2_xy[i ][1]+11, string);
				break;
			case 2:
				bmpfont_draw(g_pFont28, num2_xy[i ][0]+13, num2_xy[i ][1]+11, string);
				break;
			default:
				bmpfont_draw(g_pFont28, num2_xy[i ][0]+(4-digits) *8, num2_xy[i ][1]+11, string);
		}
	}
	flip();
}
// ====================== 마사지 End ================================//


// ====================== 환자 케어 Start ============================//
//////////////////////////////////////////////////////////////////////////////////////////////////
// 환자 케어
// 머리감기, 배변, 이동
//////////////////////////////////////////////////////////////////////////////////////////////////
bool move_flag = false;
bool left_key = false;
bool right_key = false;
bool running_flag = false;
bool tilt_care_ready = false;	// ESP32 tilt_care[] 완료 통지 수신 후 true → 좌/우 조그 UI 활성화
int  tilt_sel = 0;				// 틸팅 방향 선택 (0=좌틸팅, 1=우틸팅). ◀/▶로 이동, CONFORM 홀드로 실행
static bool care_paused = false;	// 케어모드 일시정지 상태

void patient_care_proc(void)
{
	if(smart_bed_display.status != MODE_PATIENT_CARE)
	{
		smart_bed_display.status = MODE_PATIENT_CARE;
		smart_bed_display.display_refresh = true;
		memset(&cursor, 0, sizeof(CURSOR));// key init
		cursor.set_max = 0;
		cursor.type_max = 3;	// 식사모드, 틸팅, 트렌델렌버그
		memcpy(&temp_body, &patient_care, sizeof(BODY));

		// 재진입 시 running_flag/cursor를 현재 ESP32 모드에 맞춰 복원.
		// (다른 화면 갔다 오면 상태 잃어버리던 버그 방지)
		switch(bed_status.current_mode){
			case CMD2_MEAL:
				running_flag = true;
				cursor.type = PATIENT_MEAL;
				break;
			case CMD2_READY_TILT:
				running_flag = true;
				cursor.type = PATIENT_TILT;
				// 종료 진행 중(STDBY/외부 종료)엔 분할 해제 → 단일 "틸팅" 박스로 복원.
				if(stdby_in_progress || external_stopping)
					tilt_care_ready = false;
				// [수정] tilt_care_ready를 run_state로 판정하지 않는다.
				//   올라가는 중에도 run_state=1이라, 재진입 시 아직 준비 안 됐는데도
				//   좌/우 분할이 미리 되던 문제가 있었다. 준비 완료는 오직 마스터의
				//   TILT_READY(0xF2) 수신 시에만 true가 되고(protocol.c, 화면 무관 처리),
				//   그 값이 유지되므로 재진입 시엔 유지된 값을 그대로 사용한다.
				break;
			case CMD2_TRENDELENBUG:
				running_flag = true;
				cursor.type = PATIENT_TREND;
				break;
			default:
				running_flag = false;
				tilt_care_ready = false;
				break;
		}

		remocon_key.key_val = 0xFF;
		conform_key_run = 0;
		care_paused = (bed_status.run_state == 2);
		return;
	}

	switch(remocon_key.key_val){
		case CARE_KEY:	// 돌봄케어 버튼(CARE_KEY 물리위치) 재입력 → 홈
			// [변경] 모든 돌봄 모드는 화면을 나가도(홈/다른 화면) CMD2_PAUSE를 보내지 않고
			//   계속 동작 유지(틸트처럼). → 좌상단 상태가 일시정지로 안 바뀌고, 재진입 시 복원.
			//   (동작 종료는 POWER(종료)로만.)
			// running_flag 유지 → 재진입/상태표시가 계속 동작 중
			smart_bed_status.status = MODE_HOME;
			remocon_key.key_val = 0xFF;
			break;
		case UP_KEY:
			// 동작 중이면 key_read()에서 조그 처리됨
			// 동작 안 할 때: 모드 선택 위로
			if(!running_flag) {
				if(cursor.type > 0)
					cursor.type--;
				else
					cursor.type = cursor.type_max - 1;
				smart_bed_display.display_refresh = true;
			}
			remocon_key.key_val = 0xFF;
			break;
		case DOWN_KEY:
			// 동작 안 할 때: 모드 선택 아래로
			if(!running_flag) {
				if(cursor.type < cursor.type_max - 1)
					cursor.type++;
				else
					cursor.type = 0;
				smart_bed_display.display_refresh = true;
			}
			remocon_key.key_val = 0xFF;
			break;
		case LEFT_KEY:
			// 틸팅 분할 상태: ◀ = 좌틸팅 선택(노란색 이동). 조그 아님.
			if(running_flag && tilt_care_ready && cursor.type == PATIENT_TILT){
				tilt_sel = 0;
				smart_bed_display.display_refresh = true;
			}
			remocon_key.key_val = 0xFF;
			break;
		case RIGHT_KEY:
			// 틸팅 분할 상태: ▶ = 우틸팅 선택(노란색 이동). 조그 아님.
			if(running_flag && tilt_care_ready && cursor.type == PATIENT_TILT){
				tilt_sel = 1;
				smart_bed_display.display_refresh = true;
			}
			remocon_key.key_val = 0xFF;
			break;
		case CONFORM_KEY:// 확인 (시작 / 틸팅 조그)
			// [변경] 동작 중(모든 돌봄 모드)엔 CONFORM으로 일시정지하지 않고 계속 동작 유지(틸트처럼).
			//   틸팅 분할 상태의 방향 조그는 key_read()의 CONFORM 홀드에서 처리된다.
			if(running_flag){
				remocon_key.key_val = 0xFF;
				break;
			}
			// 미동작 → 선택된 케어 모드 시작
			{
				// 시작: 선택된 케어 모드 실행 (화면 유지)
				running_flag = true;
				care_paused = false;
				tilt_care_ready = false;	// tilt_care[] 완료 통지 받기 전까지 UI 분할 X
				tilt_sel = 0;				// 분할 시 좌틸팅부터 선택(노란색)
				U8 tmp = 0;
				U8 cmd;
				if(cursor.type == PATIENT_MEAL)
					cmd = CMD2_MEAL;		// 0x46 식사모드
				else if(cursor.type == PATIENT_TILT)
					cmd = CMD2_READY_TILT;	// 0x40 틸팅
				else
					cmd = CMD2_TRENDELENBUG;		// 0x47 트렌델렌버그 (돌봄 전용 신규)
				esp32_packet_send(CMD1_SEND_RUN_ST, cmd, &tmp, 0);
				// 화면 유지 (홈으로 이동하지 않음)
			}
			smart_bed_display.display_refresh = true;
			remocon_key.key_val = 0xFF;
			break;
		case SET_KEY:
			remocon_key.key_val = 0xFF;
			break;
		default:
			remocon_key.key_val = 0xFF;
			break;
	}
}

// draw_triangle_up/down은 posture 섹션에 정의 — forward declaration
static void draw_triangle_up(int cx, int cy, int size, U32 color);
static void draw_triangle_down(int cx, int cy, int size, U32 color);

// 환자케어: 머리감기/식사/틸팅 3버튼 배치
#define CARE_BTN1_X	15		// 머리감기 버튼 X
#define CARE_BTN2_X	115		// 식사 버튼 X
#define CARE_BTN3_X	215		// 틸팅 버튼 X

// 조그 화살표 위치 (서브스크린)
#define CARE_ARROW_CX	160		// 화살표 중심 X
#define CARE_ARROW_UP_Y	250		// ▲ 꼭지점 Y
#define CARE_ARROW_DN_Y	420		// ▼ 꼭지점 Y
#define CARE_ARROW_LT_X	60		// ◀ 꼭지점 X
#define CARE_ARROW_RT_X	260		// ▶ 꼭지점 X
#define CARE_ARROW_LR_Y	340		// ◀▶ 중심 Y
#define CARE_ARROW_SIZE	25		// 화살표 크기

static void draw_triangle_left(int cx, int cy, int size, U32 color)
{
	int i;
	for(i = 0; i < size; i++){
		int w = i;
		draw_rectfill(cx + i, cy - w, 1, w * 2 + 1, color);
	}
}

static void draw_triangle_right(int cx, int cy, int size, U32 color)
{
	int i;
	for(i = 0; i < size; i++){
		int w = i;
		draw_rectfill(cx + size - 1 - i, cy - w, 1, w * 2 + 1, color);
	}
}

// PDF 슬라이드 12 — 돌봄케어 (머리감기/식사모드/틸팅모드 3 옵션)
void patient_care_draw(void)
{
	char buf[8];
	int i;
	U32 BORDER = MAKE_COLORREF(220, 50, 50);
	U32 icon_color, pill_bg;
	const char* care_labels[3] = { "식사모드", "틸팅", "거꾸리" };

	set_draw_target(getbackframe());
	draw_rectfill(0, 0, 320, 480, MAKE_COLORREF(20, 25, 38));

	// 헤더 (공용 — 온도/음량/무중력)
	draw_status_header();

	// 돌봄 타이틀 (욕창케어와 동일)
	draw_title_bar("돌봄");

	// 3개 모드 버튼 — 텍스트 박스 가로 중앙 정렬 (5~315)
	// PATIENT_TILT(i=1)만 예외: 가로 반반으로 나누어 좌틸팅/우틸팅 표시
	// [정렬] 체압분산 상단(137)~교대부양 하단(421) 구간에 3박스 균등 배치.
	//   구간 284 = 3*box_h + 2*gap → box_h=85, gap=14 → y=137/236/335 (하단 420).
	//   가로: 전폭 중앙 / 세로: 마사지·욕창 블록과 동일선.
	{
	int box_h = 86, vgap = 13;	// 3박스 균일 높이 + 하단 정확히 421 (137 + 3*86 + 2*13 = 421)
	for(i = 0; i < 3; i++) {
		int y = 137 + i * (box_h + vgap);		// 137, 236, 335
		int ty = y + (box_h - 28) / 2;			// 28pt 텍스트 세로 중앙
		bool selected = (cursor.type == i);
		U32 base_bg = selected ?
			MAKE_COLORREF(255, 230, 80) : MAKE_COLORREF(230, 230, 230);
		U32 tc = MAKE_COLORREF(20, 25, 38);
		int tw, tx;

		if(i == PATIENT_TILT && tilt_care_ready) {
			// 분할 조건: tilt_care[] 준비 완료 후에만 좌/우 분할 UI
			// 준비 전(CONFORM 후 tilt_care 실행 중)에는 아직 단일 "틸팅" 박스로 표시
			// 박스 5~315(310px), 가운데 6px 갭, 각 half 152px
			const int GAP = 6;
			const int HALF_W = (310 - GAP) / 2;	// 152
			// 선택된 방향(tilt_sel)만 강조, 나머지는 비선택.
			//   CONFORM 홀드(=선택 방향 조그 실행) 중이면 그 박스를 진한 오렌지로 강조.
			//   [전시모드] 색/테두리/글자색을 교대부양 일반 박스와 동일하게.
			U32 GRAY   = bed_status.reserved ? MAKE_COLORREF( 90,  85,  70) : MAKE_COLORREF(230, 230, 230);
			U32 YELLOW = bed_status.reserved ? MAKE_COLORREF(210, 180, 120) : MAKE_COLORREF(255, 230, 80);
			U32 ORANGE = MAKE_COLORREF(255, 140, 40);
			int radius = bed_status.reserved ? 10 : 8;
			U32 seltc = MAKE_COLORREF(30, 30, 30), unseltc = MAKE_COLORREF(225, 225, 225);
			bool jog_held = running_flag && (!(remocon_key.current & UP_KEY) || !(remocon_key.current & DOWN_KEY));
			U32 left_bg  = (tilt_sel == 0) ? (jog_held ? ORANGE : YELLOW) : GRAY;
			U32 right_bg = (tilt_sel == 1) ? (jog_held ? ORANGE : YELLOW) : GRAY;
			// 전시모드 글자색: 선택쪽=진한색, 비선택쪽=밝은색 / 일반모드: 항상 진한색
			U32 left_tc  = bed_status.reserved ? (tilt_sel == 0 ? seltc : unseltc) : tc;
			U32 right_tc = bed_status.reserved ? (tilt_sel == 1 ? seltc : unseltc) : tc;

			// 좌 half
			draw_roundrectfill(5, y, HALF_W, box_h, radius, left_bg);
			if(bed_status.reserved && tilt_sel == 0) draw_roundrect(5, y, HALF_W, box_h, radius, MAKE_COLORREF(255, 255, 255));
			egl_font_set_color(g_pFontKor, left_tc);
			tw = estimate_text_width_28("좌틸팅");
			tx = 5 + (HALF_W - tw) / 2;
			draw_text_kr(g_pFontKor, tx, ty, "좌틸팅");

			// 우 half
			int rx = 5 + HALF_W + GAP;
			draw_roundrectfill(rx, y, HALF_W, box_h, radius, right_bg);
			if(bed_status.reserved && tilt_sel == 1) draw_roundrect(rx, y, HALF_W, box_h, radius, MAKE_COLORREF(255, 255, 255));
			egl_font_set_color(g_pFontKor, right_tc);
			tw = estimate_text_width_28("우틸팅");
			tx = rx + (HALF_W - tw) / 2;
			draw_text_kr(g_pFontKor, tx, ty, "우틸팅");

				// 선택된 박스(좌/우)에 방향 표시 ▲/▼ — 텍스트(ty) 위/아래 여백에만 그림.
				//   더 올릴 수 있으면 ▲, 더 내릴 수 있으면 ▼. 전부 올라가면 ▲ 숨김, 전부 내려가면 ▼ 숨김.
				{
					static const int lbars[5] = {2,4,6,8,10};	// 좌 선택 시 제어되는 바
					static const int rbars[5] = {1,3,5,7,9};	// 우 선택 시 제어되는 바
					const int *sbars = (tilt_sel == 0) ? lbars : rbars;
					long psum = 0; int k;
					for(k = 0; k < 5; k++) psum += motor_positions[sbars[k]];
					int pos = (int)(psum / 5);					// 선택 쪽 평균 위치
					// 실제 도달 상한은 3400이 아니라 ~3300 (드라이버가 MAX_PULSE_COUNT=3300으로 클램프).
					// 하한은 틸팅 내림 목표 1260. 각 끝단에서 여유(80) 안이면 "끝까지" 도달로 보고 화살표 숨김.
					bool show_up   = (pos < 3300 - 80);			// 3220 미만 → 아직 더 올릴 수 있음 → ▲
					bool show_down = (pos > 1260 + 80);			// 1340 초과 → 아직 더 내릴 수 있음 → ▼
					int acx = (tilt_sel == 0) ? (5 + HALF_W / 2) : (rx + HALF_W / 2);
					if(show_up)   draw_arrow_up(acx, ty - 16, 16, 9, tc);	// 텍스트 위
					if(show_down) draw_arrow_down(acx, ty + 30, 16, 9, tc);	// 텍스트 아래
				}
		} else if(bed_status.reserved) {
			// [전시모드] 교대부양 일반 박스와 동일한 디자인 (베이지/회색 + 흰 테두리)
			draw_care_grid_box(5, y, 310, box_h, care_labels[i], selected);
		} else {
			// 단일 박스 — 중앙 정렬 (일반모드 기존 디자인)
			draw_roundrectfill(5, y, 310, box_h, 8, base_bg);
			egl_font_set_color(g_pFontKor, tc);
			tw = estimate_text_width_28(care_labels[i]);
			tx = 5 + (310 - tw) / 2;
			draw_text_kr(g_pFontKor, tx, ty, care_labels[i]);
		}
	}
	}

	// 푸터
	draw_home_footer(care_tag_img);

	flip();
}

// patient_care_draw_legacy() 삭제됨 (미사용, PATIENT_HEAD enum 제거로 정리)
// ====================== 환자 케어 End =============================//

// ====================== 온열 START =============================//

void heat_proc(void)
{
	U8 tmp = 0;
	
	if(smart_bed_display.status != MODE_HEAT)
	{
		smart_bed_display.status = MODE_HEAT;
		smart_bed_display.display_refresh = true;
		memset(&cursor, 0, sizeof(CURSOR));// key init
		cursor.type = heat.body[0][0];
		cursor.set_max = 0;
	    cursor.type_max = LEVIT_MAX+1;
		memcpy(&temp_body, &heat, sizeof(BODY));
		remocon_key.key_val = 0xFF;
		conform_key_run = 0;
		return;
	}
	
	switch(remocon_key.key_val){
		case HEAT_KEY:
			cursor.type++;
			if(cursor.type >= cursor.type_max)
					cursor.type = 0;// all off
			smart_bed_display.display_refresh = true;// lcd refresh
			temp_body.body[0][0] =  cursor.type;
			memcpy(&heat, &temp_body, sizeof(BODY));
			esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_HEAT + cursor.type, &tmp , 0);
			remocon_key.key_val = 0xFF;
			heat_led_ctrl(cursor.type);
			//break;
		default:
			remocon_key.key_val = 0xFF;
			break;

	}
}

void heat_draw(void)
{
	int i = 0;
	
	set_draw_target(getbackframe());// back frame select
	draw_surface(btm2_img,0,0);	
	draw_surface(heat_titile_img, TITLE_X, TITLE_Y);	
	draw_surface(heat_mian_icon_img, MAIN_ICON_X, MAIN_ICON_Y);	

	for(i = 0; i < 4; i++)
	{
		// if(cursor.type == (i-1))
			// draw_roundrectfill(50 + (i - 1) * 100-5, 162-5,   30,    30,		60, MAKE_COLORREF(0,0,255));// outer line
			//draw_circle(50 + i * 100 +10 , 162+10,		11, MAKE_COLORREF(0,0,255));// outer line
		if(temp_body.body[0][0] == (i + 1))
			draw_surface(heat_on_img, 50 + i * 100, 162);
		else
			draw_surface(heat_off_img, 50 + i * 100, 162);
		
	}
	flip();

}
// ====================== 온열 END =============================//

// 통풍(VENTILATION) 화면은 삭제됨. 해당 버튼(bit 14)은 음량 전용이며
// 음량은 화면 전환 없이 key.c에서 LED + CMD2_VOLUME 로 직접 처리한다.
BODY initial;
void initial_proc(void)
{
	if(smart_bed_display.status != MODE_INITIAL)
	{
		smart_bed_display.status = MODE_INITIAL;	// 
		smart_bed_display.display_refresh = true;//
		memset(&cursor, 0, sizeof(CURSOR));// key init
		cursor.set_max = 0;
	    cursor.type_max = 0;
		memcpy(&temp_body, &initial, sizeof(BODY));
		
		remocon_key.key_val = 0xFF;
		//body_set_max[0] = 0xC0;// time
	//	body_set_max[1] = 0x60;	// high
		conform_key_run = 0;
		return;
	}
	
	switch(remocon_key.key_val){
		case GRAVITY_KEY:
			smart_bed_status.status = MODE_HOME;
			remocon_key.key_val = 0xFF;
			break;

			
		case CONFORM_KEY:
			conform_key_proc1(CMD2_INIT + cursor.type);
			levitate_value_power_init();
			dispersion_value_power_init();
			massage_value_power_init();
			patient_care_value_power_init();
			heat_value_power_init();
			// volume_value_power_init() 제거: 음량은 재초기화해도 유지한다.
			// ESP32가 보고하는 실제 볼륨(NVS 유지값)으로 protocol.c에서 LED를 동기화한다.
			initial.body[0][0] = true;
			//memcpy(&initial, &temp_body, sizeof(BODY));
			//remocon_key.key_val = 0xFF;
		//	smart_bed_status.status = MODE_HOME;
			smart_bed_display.display_refresh = true;// 
			//break;

		default:
			remocon_key.key_val = 0xFF;
	}
}

void initial_draw(void)
{
	set_draw_target(getbackframe());

	// 배경: 진한 네이비
	draw_rectfill(0, 0, 320, 480, MAKE_COLORREF(15, 20, 35));

	// 상단 장식 라인
	if(initial.body[0][0] == true)
		draw_rectfill(0, 0, 320, 4, MAKE_COLORREF(0, 180, 80));
	else
		draw_rectfill(0, 0, 320, 4, MAKE_COLORREF(0, 150, 200));

	// 타이틀 바 (둥근 사각형)
	if(initial.body[0][0] == true)
		draw_roundrectfill(30, 25, 260, 45, 10, MAKE_COLORREF(0, 140, 65));
	else
		draw_roundrectfill(30, 25, 260, 45, 10, MAKE_COLORREF(0, 85, 135));

	egl_font_set_color(g_pFontKor, MAKE_COLORREF(255, 255, 255));
	draw_text_kr(g_pFontKor, 118, 34, "초기화");

	// 구분선
	draw_rectfill(50, 90, 220, 1, MAKE_COLORREF(50, 60, 80));

	if(initial.body[0][0] == true)
	{
		// 성공 원형 아이콘 (이중 원)
		draw_circlefill(160, 185, 45, MAKE_COLORREF(0, 160, 70));
		draw_circlefill(160, 185, 38, MAKE_COLORREF(0, 130, 55));

		// OK 텍스트
		egl_font_set_color(g_pFont28, MAKE_COLORREF(255, 255, 255));
		bmpfont_draw(g_pFont28, 143, 173, "OK");

		// 완료 메시지
		egl_font_set_color(g_pFontKor, MAKE_COLORREF(0, 220, 100));
		draw_text_kr(g_pFontKor, 55, 275, "시스템 초기화 완료");

		// 하단 안내
		draw_rectfill(50, 320, 220, 1, MAKE_COLORREF(50, 60, 80));
		egl_font_set_color(g_pFontKor16, MAKE_COLORREF(120, 130, 150));
		draw_text_kr(g_pFontKor16, 40, 340, "초기화 버튼을 눌러 돌아갑니다");
	}
	else
	{
		// 경고 원형 아이콘 (이중 원)
		draw_circlefill(160, 185, 45, MAKE_COLORREF(220, 70, 30));
		draw_circlefill(160, 185, 38, MAKE_COLORREF(190, 55, 20));

		// ! 텍스트
		egl_font_set_color(g_pFont28, MAKE_COLORREF(255, 255, 255));
		bmpfont_draw(g_pFont28, 155, 173, "!");

		// 안내 메시지
		egl_font_set_color(g_pFontKor, MAKE_COLORREF(255, 255, 255));
		draw_text_kr(g_pFontKor, 35, 275, "시스템을 초기화하려면");

		egl_font_set_color(g_pFontKor, MAKE_COLORREF(255, 80, 60));
		draw_text_kr(g_pFontKor, 25, 315, "확인 버튼을 누르세요");

		// 하단 주의 문구
		draw_rectfill(50, 375, 220, 1, MAKE_COLORREF(50, 60, 80));
		egl_font_set_color(g_pFontKor16, MAKE_COLORREF(120, 130, 150));
		draw_text_kr(g_pFontKor16, 48, 395, "모든 설정이 초기화됩니다");
	}

	flip();
}


// 
// BAR의 On/Off 상태를 읽어 온다.
// SEND : FF81 '9''B''1''C' 31 00 00 xxxx 
// ACK   : FF81 '9''B''1''C' 31 FF 10 on off ~~~ xx xx
//
/*
bool get_cmd_send = false;
void get_bar_run_info(void)
{
	U8 tmp;
	U8 buff[20]={0,};
	int i;

	if(get_cmd_send == false)
	{// command send
		get_cmd_send = true;
		esp32_packet_send(CMD1_GET_BAR_INFO, 0x10, &tmp, 0);
	}
	else
	{// ack receive
		//get_cmd_send = false;
	//esp32_packet_receive(CMD1_GET_BAR_INFO,  0x10, 12+LENGTH + 1+2, buff) ;
		if(esp32_packet_receive(CMD1_GET_BAR_INFO,  0x10, 12+LENGTH + 1+2, buff)  == true)
		{
			debugprintf("\n\r");
			memcpy(&bar, buff, 12);
			for(i = 0; i< 12; i++)
				debugprintf("%x ", bar[i]);
			get_cmd_send = false;
		}
		//get_cmd_send = false;
		//else PRINTLINE;
	}
}
*/

////////////////////// Self test /////////////////////
	
//
// self test
//
// void self_test_lcd(void)
// {
	// int i = 0;
	// set_draw_target(getbackframe());
	// draw_rectfill(0, 0, 320, 480, MAKE_COLORREF(255,255,255));
	
	// draw_rectfill(0, 0, 320, 480, MAKE_COLORREF(255,0, 0));
	// delayms(500);
	// draw_rectfill(0, 0, 320, 480, MAKE_COLORREF(0,255,0));
	// delayms(500);
	// draw_rectfill(0, 0, 320, 480, MAKE_COLORREF(0,0,255));
	// delayms(500);
	// draw_rectfill(0, 0, 320, 480, MAKE_COLORREF(255,255,255));
	// delayms(500);
	
	
// }


// ====================== 매뉴얼설정 & 자가진단 Start ================== //
//////////////////////////////////////////////////////////////////////////////////////////////////
//
// 자가진단은 설정/저장을 누르면 현재 값 Home mode로 돌아간다.
// 매뉴얼 <-> 자가진단
//
//	 REMOCON LED TEST 6개의 LED를 ON/OFF
//	AUDIO 
//	FAN
//	HEAT
//	ACTUATOR
//	BAR SENSOR
//	MOTOR
//	RPM
//	MOTOR BOARD LED
//////////////////////////////////////////////////////////////////////////////////////////////////
void down_key_self_test(CURSOR *cur)
{
	cur->type++;
	if(cur->type >= cur->type_max)
		cur->type = 0;
	debugprintf("cur type[%d]\n\r", cur->type);
	smart_bed_display.display_refresh = true;// lcd refresh
}		

void up_key_self_test(CURSOR *cur)
{
	if(cur->type)
	{
		cur->type--;
	}
	else
	{
		cur->type = cur->type_max-1;
		debugprintf("cur type[%d]\n\r", cur->type);
	}
	smart_bed_display.display_refresh = true;// lcd refresh
}
void remocon_led_test(void)
{
	heat_led_ctrl(1);
	delayms(500);
	heat_led_ctrl(2);
	delayms(500);
	heat_led_ctrl(4);
	delayms(500);
	heat_led_ctrl(7);
	delayms(500);
	
	volume_led_ctrl(1);
	delayms(500);
	volume_led_ctrl(2);
	delayms(500);
	volume_led_ctrl(4);
	delayms(500);
	volume_led_ctrl(7);
	delayms(500);

}


// #define KEY_TITLE_X 30
// #define KEY_TITLE_Y 3
// #define KEY_TITLE_W	250
// #define KEY_TITLE_H	40
// #define KEY_TITLE_TXT_X	(KEY_TITLE_X + 15)
// #define KEY_TITLE_TXT_Y	(KEY_TITLE_Y + 5)

// #define SUB_TITLE_X 10
// #define SUB_TITLE_Y 50
// #define SUB_TITLE_W	139
// #define SUB_TITLE_H	30
// #define SUB_TITLE_TXT_X	(SUB_TITLE_X + 15)
// #define SUB_TITLE_TXT_Y	(SUB_TITLE_Y + 8)

// void self_test_key(void)
// {
	// int i = 0;
	// U8 string[100] = {0, };
	
	// set_draw_target(getbackframe());// back frame select
	// draw_rectfill(0, 0, 320, 480, MAKE_COLORREF(0,0,0));
	// draw_rect(0, 0, 320, 480, MAKE_COLORREF(255,255,255));
	// draw_rect(1, 1, 320-2, 480-2, MAKE_COLORREF(255,255,255));
	
	// draw_roundrectfill(KEY_TITLE_X, KEY_TITLE_Y, KEY_TITLE_W, KEY_TITLE_H, 10, MAKE_COLORREF(0,100,150));
	// egl_font_set_color(g_pFont28, MAKE_COLORREF(255,255,255));
	// sprintf(string, "--> KEY TEST  <--");
	// bmpfont_draw(g_pFont28, KEY_TITLE_TXT_X, KEY_TITLE_TXT_Y, string);
	// flip();
	// while(1)
	// {
		// key_proc();
		
		// if(i == 14)
			// break;
	// }
// }

// --> BAR TEST <--
#define BAR_TITLE_X 30
#define BAR_TITLE_Y 3
#define BAR_TITLE_W	250
#define BAR_TITLE_H	40
#define BAR_TITLE_TXT_X	(BAR_TITLE_X + 20)
#define BAR_TITLE_TXT_Y	(BAR_TITLE_Y + 5)

// -----> BAR n <----
#define BAR_SUB_BOOX_X 35
#define BAR_SUB_BOX_Y 53
#define BAR_SUB_BOX_W	139
#define BAR_SUB_BOX_H	30
#define BAR_SUB_TXT_X	(BAR_SUB_BOOX_X + 0)
#define BAR_SUB_TXT_Y	(BAR_SUB_BOX_Y + 0)

// adc value
#define BAR_TXT_BOX_X	BAR_SUB_BOOX_X
#define BAR_TXT_BOX_Y	(BAR_SUB_BOX_Y + BAR_SUB_BOX_H)
#define BAR_TXT_BOX_W	250
#define BAR_TXT_BOX_H	20
#define BAR_TXT_X	(BAR_TXT_BOX_X + 0)
#define BAR_TXT_Y	(BAR_TXT_BOX_Y + 0)


typedef struct _self_test
{
	U16 bar[10];
	U8 send_step;
	bool time_out;
	//U8 display_page;
	
}SELF_TEST_ST;
SELF_TEST_ST self_test_val[16];

// 
// bar의 adc값을 읽어온다.
// bar에는 10개의 load cell이 있고
// bar는 총 16개가 있다.
// display에 adc값을 hex로 표시한다.
// 1화면에 3개의 bar 정보를 표시한다
// exit는 설정 키로 한다.
//

enum _test_mode{
	BAR_TEST_1_3 = 0,
	BAR_TEST_4_6,
	BAR_TEST_7_9,
	BAR_TEST_10_12,
	BAR_TEST_13_15,
	BAR_TEST_16,
	
	MOTOR_TEST,
	RPM_TEST,
	LED_TEST,
	TEST_MAX
};

//
// 1~3,4~6,7~9,10~12,14~15, 16

//


void self_test_bar_draw(U8 bar_line, int bar_cnt, bool ready)
{
	int i = 0;
	U8 string[200] = {0, };
	memset(&self_test_val[bar_line],0,sizeof(SELF_TEST_ST));
	set_draw_target(getbackframe());// back frame select
	draw_rectfill(0, 0, 320, 480, MAKE_COLORREF(0,0,0));
	draw_rect(0, 0, 320, 480, MAKE_COLORREF(255,255,255));
	draw_rect(1, 1, 320-2, 480-2, MAKE_COLORREF(255,255,255));
	
	draw_roundrectfill(BAR_TITLE_X, BAR_TITLE_Y, BAR_TITLE_W, BAR_TITLE_H, 10, MAKE_COLORREF(0,100,150));
	egl_font_set_color(g_pFont28, MAKE_COLORREF(255,255,255));
	sprintf(string, "--> BAR TEST  <--");
	bmpfont_draw(g_pFont28, BAR_TITLE_TXT_X, BAR_TITLE_TXT_Y, string);
	
	//draw_roundrectfill(SUB_TITLE_X, SUB_TITLE_Y, SUB_TITLE_W, SUB_TITLE_H, 40, MAKE_COLORREF(255, 255, 255));
	for(i=0; i<bar_cnt; i++){
		//draw_rectfill(BAR_TITLE_X, BAR_TITLE_Y, BAR_TITLE_W, BAR_TITLE_H, MAKE_COLORREF(255,255,255));
		egl_font_set_color(g_pFont28, MAKE_COLORREF(255,255,255));
		sprintf(string, "------> BAR %d <------", bar_line+ i + 1);
		bmpfont_draw(g_pFont28, BAR_SUB_TXT_X, BAR_SUB_TXT_Y + i*(BAR_SUB_BOX_H + 100), string);
		
		sprintf(string, "SEN1,  SEN2,  SEN3,  SEN4,  SEN5");
		bmpfont_draw(g_pFont16, BAR_TXT_X, BAR_SUB_TXT_Y + i*(BAR_SUB_BOX_H + 100) +BAR_SUB_BOX_H , string);
		
		sprintf(string, "SEN6,  SEN7,  SEN8,  SEN9,  SEN10");
		bmpfont_draw(g_pFont16, BAR_TXT_X, BAR_SUB_TXT_Y + i*(BAR_SUB_BOX_H + 100) + BAR_SUB_BOX_H + BAR_TXT_BOX_H*2 , string);
			
		if(ready & (1<<i))
		{
			memset(string, 0, sizeof(string));
			//sprintf(string, "%04x, %04x, %04x, %04x, %04x", self_test_val.bar[0][0], self_test_val.bar[0][1], self_test_val.bar[0][2], self_test_val.bar[0][3], self_test_val.bar[0][4]);
			sprintf(string, "%04x, %04x, %04x, %04x, %04x", self_test_val[bar_line+i].bar[0], self_test_val[bar_line+i].bar[1], self_test_val[bar_line+i].bar[2], self_test_val[bar_line+i].bar[3], self_test_val[bar_line+i].bar[4]);
			bmpfont_draw(g_pFont16, BAR_TXT_X, BAR_SUB_TXT_Y + i*(BAR_SUB_BOX_H + 100) + BAR_SUB_BOX_H + BAR_TXT_BOX_H, string);
		
			
			memset(string, 0, sizeof(string));
			//sprintf(string, "%04x, %04x, %04x, %04x, %04x", self_test_val.bar[0][0], self_test_val.bar[0][1], self_test_val.bar[0][2], self_test_val.bar[0][3], self_test_val.bar[0][4]);
			sprintf(string, "%04x, %04x, %04x, %04x, %04x", self_test_val[bar_line+i].bar[5], self_test_val[bar_line+i].bar[6], self_test_val[bar_line+i].bar[7], self_test_val[bar_line+i].bar[8], self_test_val[bar_line+i].bar[9]);
			bmpfont_draw(g_pFont16, BAR_TXT_X, BAR_SUB_TXT_Y + i*(BAR_SUB_BOX_H + 100) + BAR_SUB_BOX_H + BAR_TXT_BOX_H*3, string);
		}
		else
		{
			egl_font_set_color(g_pFontKor16, MAKE_COLORREF(255,255,255));
			draw_text_kr(g_pFontKor16, BAR_TXT_X, BAR_SUB_TXT_Y + i*(BAR_SUB_BOX_H + 100) + BAR_SUB_BOX_H + BAR_TXT_BOX_H, "대기");
			draw_text_kr(g_pFontKor16, BAR_TXT_X, BAR_SUB_TXT_Y + i*(BAR_SUB_BOX_H + 100) + BAR_SUB_BOX_H + BAR_TXT_BOX_H*3, "대기");
		}
	}
	egl_font_set_color(g_pFont28, MAKE_COLORREF(255,255,100));
	//sprintf(string, "READ PAGE %d/6", self_test_val.display_page+1);
	egl_font_set_color(g_pFontKor, MAKE_COLORREF(255,255,100));
	draw_text_kr(g_pFontKor, BAR_TXT_X+20, BAR_SUB_TXT_Y + 2*(BAR_SUB_BOX_H + 100) + BAR_SUB_BOX_H + BAR_TXT_BOX_H*3+40, "종료 : 설정/저장 키");
	flip();
}



// 총 5개의 명령어 데이터를 순서대로 처리한다.

bool self_test_bar_send_cmd(U8 bar_line, int bar_cnt)
{
	U8 tmp = 0;
	bool send_flag = false;
	int send_cnt;
	U16 rx_time_cnt = 0;

	for(send_cnt = 0; send_cnt < 5;)
	{
		// send command
		if(send_flag == false)
		{
			esp32_packet_send(CMD1_SEND_GET_BAR, bar_line<<4, &tmp, 0);
			rx_time_cnt = *((volatile unsigned int*)TMCNT_ADDR(SYS_TIMER_CH));
		}
		else
		{
			// if(esp32_packet_receive(CMD1_SEND_GET_BAR,  bar_line<<4, 20+LENGTH + 1+2, buff)  == true)
				// return true;
			// else
			// {
				// if(time_10msec_interval_get(rx_time_cnt)>100)//100msec waite
					// send_cnt++;
			// }
		}
	}
	return false;
}

void self_test_bar_proc(U8 bar_line, int bar_cnt)
{
	int i;
	
	U8 send_cnt = 0;
	bool send_end = false;
	U8 bar_ready = 0;
	CURSOR bar_cur;
	
	memset(&bar_cur, 0, sizeof(CURSOR));
	for(i = 0; i<bar_cnt; i++)
		memset(&self_test_val[i+bar_line*3], 0, sizeof(SELF_TEST_ST));
	remocon_key.key_val = 0xFF;
	smart_bed_display.display_refresh  = true;
	
	while(1)
	{
// KEY READ
		key_read();
		switch(remocon_key.key_val){
			case SET_KEY:
				remocon_key.key_val = 0xFF;
				return;
			default:
				remocon_key.key_val = 0xFF;
		}
		if(send_end == false)
		{
			// send command

			// receive bar data
		}
		else
		{
		}
// display
		if(smart_bed_display.display_refresh == true)
		{
			self_test_bar_draw(bar_line, bar_cnt, bar_ready);
			smart_bed_display.display_refresh = false;
		}
	}
}

U8 test_menu[TEST_MAX][100] = {
	{"BAR TEST 1 ~ 3"},
	{"BAR TEST 4 ~ 6"},
	{"BAR TEST 7 ~ 9"},
	{"BAR TEST 10 ~ 12"},
	{"BAR TEST 13 ~ 15"},
	{"BAR TEST 16"},
	{"MOTOR"},
	{"RPM"},
	{"LED"},
};
#define SEL_BOX_X	20
#define SEL_BOX_Y	50
#define SEL_BOX_W	280
#define SEL_BOX_H	25

void self_test_main_draw(U8 select_line)
{
	int i;

	set_draw_target(getbackframe());// back frame select
	draw_rectfill(0, 0, 320, 480, MAKE_COLORREF(0,0,0));
	draw_rect(0, 0, 320, 480, MAKE_COLORREF(255,255,255));
	draw_rect(1, 1, 320-2, 480-2, MAKE_COLORREF(255,255,255));
	
	draw_rectfill(20, 5, 280, 40, MAKE_COLORREF(155,100,155));
	egl_font_set_color(g_pFontKor, MAKE_COLORREF(0,00,0));
	draw_text_kr(g_pFontKor, 30, 10, "***** 자가 진단 *****");
	
	for(i = 0; i<TEST_MAX; i++)
	{
		if(i == select_line)
		{
			//memset(string, 0, sizeof(string));
			draw_rectfill(SEL_BOX_X , SEL_BOX_Y + (i * SEL_BOX_H), SEL_BOX_W, SEL_BOX_H, MAKE_COLORREF(0,0,255));
			egl_font_set_color(g_pFont16, MAKE_COLORREF(255,255,255));
			//sprintf(string, "SEFL TEST");
			bmpfont_draw(g_pFont16, SEL_BOX_X + 30, SEL_BOX_Y + (i * SEL_BOX_H) + 5, test_menu[i]);
			//debugprintf("\n\rtrue:%s i[%d], select_line[%d]",test_menu[i], i, select_line);
		}
		else
		{
			egl_font_set_color(g_pFont16, MAKE_COLORREF(255,255,255));
			//sprintf(string, "SEFL TEST");
			bmpfont_draw(g_pFont16, SEL_BOX_X + 30, SEL_BOX_Y + (i * SEL_BOX_H) + 5, test_menu[i]);
			//debugprintf("\n\rfalse:%s i[%d], select_line[%d]", test_menu[i], i, select_line);
		}
	}
	flip();
}

// 
// 메뉴는 UP/DOWN KEY로 선택한다.
// 실행은 확인 키로 실행한다.
// EXIT는 설정 KEY로 EXIT한다.
//
void self_test_main(void)
{
	//U8 disp_ref = true;
	CURSOR self_cur;
	
	memset(&self_cur, 0, sizeof(CURSOR));
	self_cur.type_max = TEST_MAX;
	smart_bed_display.display_refresh  = true;
	remocon_key.key_val = 0xFF;
	while(1)
	{
		key_read();
		switch(remocon_key.key_val){
			case SET_KEY:
				remocon_key.key_val = 0xFF;
				return;
				//break;
			case UP_KEY:		
				up_key_self_test(&self_cur);
				remocon_key.key_val = 0xFF;
				debugprintf("\n\r %d", self_cur.type);
				break;
			case DOWN_KEY:	
				down_key_self_test(&self_cur);
				remocon_key.key_val = 0xFF;
			debugprintf("\n\r %d", self_cur.type);
				break;
				
			case CONFORM_KEY:
				switch(self_cur.type)
				{
					case BAR_TEST_1_3:
						self_test_bar_proc(BAR_TEST_1_3, 3);
						break;
						
				}
		//		if(cursor.type == SELF_TEST)
				//	self_test();
				remocon_key.key_val = 0xFF;
				smart_bed_display.display_refresh  = true;
				break;
			// case SET_KEY:
				// //set_key_proc(CMD2_DISPERSION, dispersion.body[cursor.body], sizeof(dispersion.body[cursor.body]));	// 일반
				// remocon_key.key_val = 0xFF;
				// break;
			default:
				remocon_key.key_val = 0xFF;
		}
		if(smart_bed_display.display_refresh == true)
		{
			self_test_main_draw(self_cur.type);
			smart_bed_display.display_refresh = false;
		}
	}
}

#define USER_MANUAL	0
#define SELF_TEST 1
void manual_selft_test_draw(void);
BODY set;
U8 set_mode = USER_MANUAL;
U8 set_manual_sel = 0;        // 사용자 매뉴얼 탭 세로 리스트 행 커서 (0=사용자 감지(ㄱ))
#define SET_MANUAL_ROWS 5     // ㄱ.사용자감지  ㄴ~ㅁ.매뉴얼 테스트
void manual_selft_test_proc(void)
{
	if(smart_bed_display.status != MODE_SET_SAVE)
	{
		smart_bed_display.status = MODE_SET_SAVE;
		smart_bed_display.display_refresh = true;
		memset(&cursor, 0, sizeof(CURSOR));// key init
		cursor.type_max = 2;  // 탭: 0=사용자 매뉴얼, 1=자가 진단 (LEFT/RIGHT 전환)
		memcpy(&temp_body, &dispersion, sizeof(BODY));// 현재 설정 임시 버퍼로 복사

		remocon_key.key_val = 0xFF;

		conform_key_run = 0;
		set_mode = USER_MANUAL;
		set_manual_sel = 0;   // 리스트 행 커서 맨 위로
		memset(&self_test_val, 0, sizeof(SELF_TEST_ST));
		return;
	}
	
	switch(remocon_key.key_val){
		case SET_KEY:
			smart_bed_status.status = MODE_HOME;
			remocon_key.key_val = 0xFF;
			break;
		case LEFT_KEY:
			left_key_proc1();           // 탭 전환 (사용자 매뉴얼 ↔ 자가 진단)
			set_manual_sel = 0;         // 탭 바꾸면 리스트 행 커서 리셋
			remocon_key.key_val = 0xFF;
			break;
		case RIGHT_KEY:
			right_key_proc1();          // 탭 전환
			set_manual_sel = 0;
			remocon_key.key_val = 0xFF;
			break;

		case UP_KEY:                    // 사용자 매뉴얼 리스트 위로
			if(cursor.type == USER_MANUAL){
				if(set_manual_sel > 0) set_manual_sel--;
				else set_manual_sel = SET_MANUAL_ROWS - 1;
				smart_bed_display.display_refresh = true;
			}
			remocon_key.key_val = 0xFF;
			break;
		case DOWN_KEY:                  // 사용자 매뉴얼 리스트 아래로
			if(cursor.type == USER_MANUAL){
				if(set_manual_sel < SET_MANUAL_ROWS - 1) set_manual_sel++;
				else set_manual_sel = 0;
				smart_bed_display.display_refresh = true;
			}
			remocon_key.key_val = 0xFF;
			break;

		case CONFORM_KEY:
			if(cursor.type == USER_MANUAL && set_manual_sel == 0)
			{	// ㄱ. 사용자 감지 자동종료 토글
				U8 newval = g_presence_detect ? 0 : 1;
				U8 tmp = 0;
				esp32_packet_send(CMD1_SEND_RUN_ST,
					newval ? CMD2_PRESENCE_ON : CMD2_PRESENCE_OFF, &tmp, 0);
				g_presence_detect = newval;  // 낙관적 갱신 — 다음 BedStatus echo로 확정
				smart_bed_display.display_refresh = true;
			}
			else if(cursor.type == SELF_TEST)
			{
				self_test_main();
				smart_bed_display.display_refresh = true;
				PRINTLINE;
				manual_selft_test_draw();
			}
			remocon_key.key_val = 0xFF;
			break;
		// case SET_KEY:
			// //set_key_proc(CMD2_DISPERSION, dispersion.body[cursor.body], sizeof(dispersion.body[cursor.body]));	// 일반
			// remocon_key.key_val = 0xFF;
			// break;
		default:
			remocon_key.key_val = 0xFF;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//
// 매뉴얼 및 자가 테스트 Display
// 매뉴얼, 자가 테스트는 LEFT, RIGHT Key로 이동한다.
// 자가 테스트는 확인 Key로 실행한다.
//  - 에러가 없는 경우 "TEST OK"
//  - 에러가 발생했을 경우 "ERROR LIST"를 표시한다.
//
//////////////////////////////////////////////////////////////////////////////////////////////////

#define MAIN_TITLE_X 60
#define MAIN_TITLE_Y 3
#define MAIN_TITLE_W	200
#define MAIN_TITLE_H	40
#define MAIN_TITLE_TXT_X	(MAIN_TITLE_X + 35)
#define MAIN_TITLE_TXT_Y	(MAIN_TITLE_Y + 10)

#define SUB_TITLE_X 10
#define SUB_TITLE_Y 110	// 상단 헤더(0~50) + "설정" 타이틀 바(55~99) 아래로
#define SUB_TITLE_W	139
#define SUB_TITLE_H	30
#define SUB_TITLE_TXT_X	(SUB_TITLE_X + 15)
#define SUB_TITLE_TXT_Y	(SUB_TITLE_Y + 8)

void manual_selft_test_draw(void){
	set_draw_target(getbackframe());// back frame select
	draw_rectfill(0, 0, 320, 480, MAKE_COLORREF(0,0,0));

	// 상단 상태 헤더 (좌: 모드상태 / 우: 온도·음량) — 다른 화면과 동일
	draw_status_header();
	// "설정" 타이틀 바 — 다른 화면과 동일
	draw_title_bar("설정");

	// --- 상단 탭: 사용자 매뉴얼 / 자가 진단 (선택된 탭만 채움) ---
	if(cursor.type == USER_MANUAL)
		draw_roundrectfill(SUB_TITLE_X, SUB_TITLE_Y, SUB_TITLE_W, SUB_TITLE_H, 40, MAKE_COLORREF(255,255,255));
	else
		draw_roundrect(SUB_TITLE_X, SUB_TITLE_Y, SUB_TITLE_W, SUB_TITLE_H, 40, MAKE_COLORREF(255,255,255));
	egl_font_set_color(g_pFontKor16, (cursor.type==USER_MANUAL)?MAKE_COLORREF(0,0,0):MAKE_COLORREF(255,255,255));
	draw_text_kr(g_pFontKor16, SUB_TITLE_TXT_X, SUB_TITLE_TXT_Y, "사용자 매뉴얼");

	if(cursor.type == SELF_TEST)
		draw_roundrectfill(SUB_TITLE_X + SUB_TITLE_W + 20, SUB_TITLE_Y, SUB_TITLE_W, SUB_TITLE_H, 40, MAKE_COLORREF(255,255,255));
	else
		draw_roundrect(SUB_TITLE_X + SUB_TITLE_W + 20, SUB_TITLE_Y, SUB_TITLE_W, SUB_TITLE_H, 40, MAKE_COLORREF(255,255,255));
	egl_font_set_color(g_pFontKor16, (cursor.type==SELF_TEST)?MAKE_COLORREF(0,0,0):MAKE_COLORREF(255,255,255));
	draw_text_kr(g_pFontKor16, SUB_TITLE_TXT_X + SUB_TITLE_W+30, SUB_TITLE_TXT_Y, "자가 진단");

	// --- 본문: 선택된 탭별 내용 ---
	if(cursor.type == USER_MANUAL)
	{
		// 세로 리스트: ㄱ.사용자 감지(토글)  ㄴ~ㅁ.매뉴얼 테스트 — UP/DOWN 이동, 확인(CONFORM) 선택
		const char *labels[SET_MANUAL_ROWS] = {
			"사용자 감지", "매뉴얼 테스트", "매뉴얼 테스트",
			"매뉴얼 테스트", "매뉴얼 테스트"
		};
		int lx = 10, lw = 300, ly0 = SUB_TITLE_Y + 45, rh = 40, gap = 4;
		int r;
		for(r = 0; r < SET_MANUAL_ROWS; r++)
		{
			int ry = ly0 + r*(rh + gap);
			if(r == set_manual_sel)
				draw_roundrectfill(lx, ry, lw, rh, 10, MAKE_COLORREF(40,90,160));   // 선택 강조
			else
				draw_roundrect(lx, ry, lw, rh, 10, MAKE_COLORREF(90,90,100));
			egl_font_set_color(g_pFontKor16, MAKE_COLORREF(255,255,255));
			draw_text_kr(g_pFontKor16, lx + 16, ry + 11, labels[r]);
			if(r == 0)   // ㄱ 행: 켜짐/꺼짐 값 박스
			{
				int vx = lx + lw - 92, vw = 80, vy = ry + 5, vh = 30;
				U8  on = (g_presence_detect != 0);
				if(on) draw_roundrectfill(vx, vy, vw, vh, 9, MAKE_COLORREF(40,160,80));
				else   draw_roundrectfill(vx, vy, vw, vh, 9, MAKE_COLORREF(90,90,95));
				egl_font_set_color(g_pFontKor16, MAKE_COLORREF(255,255,255));
				draw_text_kr(g_pFontKor16, vx + 23, vy + 7, on ? "켜짐" : "꺼짐");
			}
		}
	}
	else if(cursor.type == SELF_TEST)
	{
		egl_font_set_color(g_pFontKor16, MAKE_COLORREF(255,255,255));
		draw_text_kr(g_pFontKor16, SUB_TITLE_TXT_X, SUB_TITLE_TXT_Y + 100, "자가 진단을 실행하시겠습니까?");
		draw_text_kr(g_pFontKor16, SUB_TITLE_TXT_X, SUB_TITLE_TXT_Y + 150, "확인 버튼을 눌러주세요!!");
	}

	// 푸터
	draw_home_footer(set_tag_img);

	flip();
}
// ====================== 매뉴얼설정 & 자가진단 End ================= //


// ====================== 자세제어 Start ================= //

// 탭 버튼 좌표 (X는 posture_draw에서 화면 폭 4등분으로 계산)
// 타이틀 바(y=55~99) 아래로 배치
#define POSTURE_TAB_Y		105

// 아이콘 좌표
#define POSTURE_ICON_X		50
#define POSTURE_ICON_Y		259		// ▲그림▼ 묶음을 선택바(149)~푸터(458) 세로 중앙에 배치 (아이콘 259~349, 중심 304)

// 화살표 좌표
#define POSTURE_ARROW_CX	160		// 화살표 중심 X
#define POSTURE_ARROW_UP_Y	189		// ▲ 꼭지점 Y — ▲그림▼ 묶음 세로 중앙 배치
#define POSTURE_ARROW_DN_Y	394		// ▼ 꼭지점 Y — 아이콘 중심(304) 기준 ▲와 대칭
#define POSTURE_ARROW_SIZE	25		// 화살표 크기 (높이)

static void draw_triangle_up(int cx, int cy, int size, U32 color)
{
	int i;
	for(i = 0; i < size; i++){
		int w = i;
		draw_rectfill(cx - w, cy + i, w * 2 + 1, 1, color);
	}
}

static void draw_triangle_down(int cx, int cy, int size, U32 color)
{
	int i;
	for(i = 0; i < size; i++){
		int w = i;
		draw_rectfill(cx - w, cy + size - 1 - i, w * 2 + 1, 1, color);
	}
}

// 높이조절 아이콘 — 침대(매트리스+사람) 밑에 평평한 프레임. ▲▼로 침대 전체 높이 up/down.
static void draw_height_bed_icon(void)
{
	int by = POSTURE_ICON_Y;	// 플레이트 아이콘과 동일 기준 — 세로 위치 함께 이동
	U32 FRAME = MAKE_COLORREF(200, 200, 210);	// 침대 프레임(회색)
	U32 MAT   = MAKE_COLORREF(210, 180, 120);	// 매트리스(베이지)
	U32 BODY  = MAKE_COLORREF(240, 240, 240);	// 사람(흰색)

	// 사람 (누운 실루엣): 머리(원) + 몸통
	draw_roundrectfill(92, by + 4, 24, 24, 12, BODY);		// 머리
	draw_roundrectfill(112, by + 10, 118, 18, 9, BODY);		// 몸통

	// 매트리스 (사람 아래)
	draw_roundrectfill(70, by + 30, 180, 24, 6, MAT);

	// 평평한 침대 프레임 (매트리스 아래, 다리 포함)
	draw_roundrectfill(64, by + 58, 192, 8, 3, FRAME);		// 상단 레일
	draw_rectfill(74, by + 66, 8, 20, FRAME);				// 좌측 다리
	draw_rectfill(238, by + 66, 8, 20, FRAME);				// 우측 다리
}

void posture_proc(void)
{
	if(smart_bed_display.status != MODE_POSTURE){
		smart_bed_display.status = MODE_POSTURE;
		smart_bed_display.display_refresh = true;
		cursor.type = POSTURE_ALL;
		// [variant] AnyBed는 높이 모터 없음 → '높이' 탭(마지막) 제외
		cursor.type_max = g_bed_variant ? POSTURE_TYPE_MAX : (POSTURE_TYPE_MAX - 1);
		remocon_key.key_val = 0xFF;
		return;
	}

	U8 tmp = 0;

	switch(remocon_key.key_val){
		case LEFT_KEY:
			if(cursor.type > 0)
				cursor.type--;
			else
				cursor.type = cursor.type_max - 1;   // variant별 탭 수 반영
			smart_bed_display.display_refresh = true;
			break;

		case RIGHT_KEY:
			cursor.type++;
			if(cursor.type >= cursor.type_max)       // variant별 탭 수 반영
				cursor.type = 0;
			smart_bed_display.display_refresh = true;
			break;

		// UP/DOWN은 key_read()에서 누름/뗌 실시간 감지로 처리
		// (누르는 동안 모터 동작, 떼면 정지)

		case CONFORM_KEY:
			// 모터 전체 정지 (안전)
			esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_PAUSE_POSTURE_BACK, &tmp, 0);
			esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_PAUSE_POSTURE_LEG, &tmp, 0);
			esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_PAUSE_HEIGHT, &tmp, 0);
			debugprintf("\n\r POSTURE: ALL STOP (incl height)");
			break;
	}
	remocon_key.key_val = 0xFF;
}

void posture_draw(void)
{
	set_draw_target(getbackframe());

	// 배경 — 그라데이션 이미지 대신 다른 화면과 동일한 단색
	draw_rectfill(0, 0, 320, 480, MAKE_COLORREF(20, 25, 38));

	// 상단 상태 헤더 (좌: 모드상태 / 우: 온도·음량)
	draw_status_header();

	// 타이틀 바 (다른 화면과 동일)
	draw_title_bar("자세제어");

	// 자세 선택 바 — 하나의 바를 |로 4구간(등/다리/등다리/높이) 구분, 선택 구간만 강조
	{
		static const char* tab_labels[POSTURE_TYPE_MAX] = {
			"등", "다리", "등다리", "높이"
		};
		const int PBAR_X = 5, PBAR_W = 310, PBAR_H = 44;
		const int PBAR_Y = POSTURE_TAB_Y;
		U32 UNSEL = MAKE_COLORREF( 90,  85,  70);	// 다른 화면 박스 비선택색과 동일
		U32 SEL   = MAKE_COLORREF(210, 180, 120);	// 다른 화면 박스 선택색과 동일
		U32 DIV   = MAKE_COLORREF(150, 160, 175);
		int t;
		// [variant] AnyBed는 '높이' 탭(마지막) 제외 → 탭 수 4→3
		int ptab = g_bed_variant ? POSTURE_TYPE_MAX : (POSTURE_TYPE_MAX - 1);

		// 1) 전체 바 (비선택 색) — 라운드 8 (타이틀 바와 동일)
		draw_roundrectfill(PBAR_X, PBAR_Y, PBAR_W, PBAR_H, 8, UNSEL);

		// 2) 선택 구간 강조 (|를 기준으로 그 칸만). 끝 칸은 바깥쪽만 라운드(8), 안쪽 각지게.
		{
			int sx0 = PBAR_X + (cursor.type * PBAR_W) / ptab;
			int sx1 = PBAR_X + ((cursor.type + 1) * PBAR_W) / ptab;
			int sw  = sx1 - sx0;
			if(cursor.type == 0){
				draw_roundrectfill(sx0, PBAR_Y, sw, PBAR_H, 8, SEL);
				draw_rectfill(sx1 - 8, PBAR_Y, 8, PBAR_H, SEL);		// 안쪽(오른) 각지게
			} else if(cursor.type == ptab - 1){
				draw_roundrectfill(sx0, PBAR_Y, sw, PBAR_H, 8, SEL);
				draw_rectfill(sx0, PBAR_Y, 8, PBAR_H, SEL);			// 안쪽(왼) 각지게
			} else {
				draw_rectfill(sx0, PBAR_Y, sw, PBAR_H, SEL);			// 중간 칸: 사각
			}
		}

		// 3) 구분선 | — 박스 높이에 안 닿게 위아래 띄우고 세로 중앙
		{
			const int DIV_H = 24, DIV_W = 2;
			int dy = PBAR_Y + (PBAR_H - DIV_H) / 2;	// 위아래 10px 여백, 세로 중앙
			int k;
			for(k = 1; k < ptab; k++){
				int dx = PBAR_X + (k * PBAR_W) / ptab;
				draw_rectfill(dx - DIV_W / 2, dy, DIV_W, DIV_H, DIV);
			}
		}

		// 4) 라벨 — 각 구간 중앙 (선택 칸: 진한 글자 / 비선택: 밝은 글자, 다른 화면과 동일)
		for(t = 0; t < ptab; t++){
			int cx0 = PBAR_X + (t * PBAR_W) / ptab;
			int cx1 = PBAR_X + ((t + 1) * PBAR_W) / ptab;
			int tw  = kr_text_width_16(tab_labels[t]);
			egl_font_set_color(g_pFontKor16, (cursor.type == t) ?
				MAKE_COLORREF(30, 30, 30) : MAKE_COLORREF(225, 225, 225));
			draw_text_kr(g_pFontKor16, cx0 + (cx1 - cx0 - tw) / 2,
				PBAR_Y + (PBAR_H - 16) / 2, tab_labels[t]);
		}
	}

	// 그림 영역 테두리 (아이콘만 감쌈 — 화살표 제외). POSTURE_ICON_Y 기준으로 함께 이동.
	draw_roundrect(POSTURE_ICON_X - 15, POSTURE_ICON_Y - 10, 246, 110, 10,
		MAKE_COLORREF(150, 160, 175));

	// 침대 아이콘 (선택된 판에 해당하는 아이콘)
	switch(cursor.type){
		case POSTURE_BACK:
			draw_surface(posture_back_plate_icon_img, POSTURE_ICON_X, POSTURE_ICON_Y);
			break;
		case POSTURE_LEG:
			draw_surface(posture_leg_plate_icon_img, POSTURE_ICON_X, POSTURE_ICON_Y);
			break;
		case POSTURE_ALL:
			draw_surface(posture_all_plate_icon_img, POSTURE_ICON_X, POSTURE_ICON_Y);
			break;
		case POSTURE_HEIGHT:
			// 높이 조절: 침대(매트리스+사람) 밑에 평평한 프레임 — ▲▼로 전체 높이 up/down
			draw_height_bed_icon();
			break;
	}

	// ▲▼ 화살표 — 상/하한에서 숨김, 누르는 중이면 강조색 (틸팅과 동일 개념)
	{
		U8 bp = bed_status.backrest_percent;
		U8 lp = bed_status.leg_panel_percent;
		// [여유값] 물리 정지 위치가 100%/0%에 딱 안 닿아도 상/하한으로 간주 (모터 스톨 위치 편차 흡수).
		//   예: 다리판 실측 최대 ≈ 99% → HI(97) 이상이면 ▲ 숨김.
		const U8 HI = 97, LO = 3;
		bool show_up, show_down;
		switch(cursor.type){
			case POSTURE_BACK: show_up = (bp < HI); show_down = (bp > LO); break;
			case POSTURE_LEG:  show_up = (lp < HI); show_down = (lp > LO); break;
			case POSTURE_ALL:  show_up = (bp < HI || lp < HI); show_down = (bp > LO || lp > LO); break;
			default: {	// POSTURE_HEIGHT — height_percent 사용 (NineBed)
				U8 hp = bed_status.height_percent;
				show_up = (hp < HI); show_down = (hp > LO);
				break;
			}
		}
		bool up_held   = !(remocon_key.current & UP_KEY);
		bool down_held = !(remocon_key.current & DOWN_KEY);
		U32 NORMAL = MAKE_COLORREF( 50, 120, 220);	// 기본 파랑
		U32 PRESS  = MAKE_COLORREF(255, 140,  40);	// 눌림 강조 (오렌지)

		if(show_up)
			draw_triangle_up(POSTURE_ARROW_CX, POSTURE_ARROW_UP_Y, POSTURE_ARROW_SIZE,
				up_held ? PRESS : NORMAL);
		if(show_down)
			draw_triangle_down(POSTURE_ARROW_CX, POSTURE_ARROW_DN_Y, POSTURE_ARROW_SIZE,
				down_held ? PRESS : NORMAL);
	}

	// 푸터
	draw_home_footer(posture_tag_img);

	flip();
}

// ====================== 자세제어 End ================= //


// ====================== 욕창케어 (체압분산 + 교대부양 통합) Start ============= //
// 커서 항목: 0=체압분산, 1=교대부양 일반, 2=집중, 3=수면
enum { ULCER_DISP = 0, ULCER_LV_NORMAL, ULCER_LV_FOCUS, ULCER_LV_SLEEP, ULCER_TYPE_MAX };

void ulcer_care_proc(void)
{
	if(smart_bed_display.status != MODE_ULCER_CARE){
		smart_bed_display.status = MODE_ULCER_CARE;
		smart_bed_display.display_refresh = true;
		memset(&cursor, 0, sizeof(CURSOR));	// cursor.mode=CURSOR_MODE, type=0(체압분산)
		// [전시모드] reserved=1 → 교대부양은 일반만 (집중/수면 숨김) → 커서 항목 2개(체압분산+일반)
		cursor.type_max = bed_status.reserved ? (ULCER_LV_NORMAL + 1) : ULCER_TYPE_MAX;
		conform_key_run = 0;
		remocon_key.key_val = 0xFF;
		ulcer_config_mode = 0;	// 설정창 닫힌 상태로 시작
		ulcer_config_anim = 0;
		return;
	}

	// 사용자맞춤 설정창 열림(마사지 설정과 동일): 위/아래=항목 이동, 좌/우=값 변경, 욕창케어/설정=닫기.
	if(!bed_status.reserved && ulcer_config_mode){
		int* pv = (ulcer_setting_focus == 0) ? &ulcer_intensity : &ulcer_speed;	// 2항목: 높이/속도
		switch(remocon_key.key_val){
			case ULCER_KEY:
			case SET_KEY:	// 닫기(내려감)
				ulcer_config_mode = 0;
				break;
			case UP_KEY:
			case DOWN_KEY:
				ulcer_setting_focus ^= 1;	// 높이 ↔ 속도 토글
				break;
			case LEFT_KEY:
				if(*pv > 0) (*pv)--;
				break;
			case RIGHT_KEY:
				if(*pv < 2) (*pv)++;
				break;
		}
		smart_bed_display.display_refresh = true;
		remocon_key.key_val = 0xFF;
		return;
	}

	switch(remocon_key.key_val){
		case ULCER_KEY:	// 욕창케어 버튼 재입력 → 홈
			smart_bed_status.status = MODE_HOME;
			break;
		case UP_KEY:
			if(bed_status.reserved){
				cursor.type = ULCER_DISP;			// 전시: 체압분산/일반 2항목
			} else {
				// 일반: 세로 리스트 4항목 선형 이동(위, 맨 위에서 맨 아래로 순환)
				cursor.type = (cursor.type > ULCER_DISP) ? (U8)(cursor.type - 1) : ULCER_LV_SLEEP;
			}
			smart_bed_display.display_refresh = true;
			break;
		case DOWN_KEY:
			if(bed_status.reserved){
				if(cursor.type == ULCER_DISP)
					cursor.type = ULCER_LV_NORMAL;
			} else {
				// 일반: 세로 리스트 4항목 선형 이동(아래, 맨 아래에서 맨 위로 순환)
				cursor.type = (cursor.type < ULCER_LV_SLEEP) ? (U8)(cursor.type + 1) : ULCER_DISP;
			}
			smart_bed_display.display_refresh = true;
			break;
		case LEFT_KEY:		// 교대부양 3개 좌 순환 (전시모드는 일반만 → 순환 없음)
			if(!bed_status.reserved && cursor.type >= ULCER_LV_NORMAL)
				cursor.type = (cursor.type > ULCER_LV_NORMAL) ? cursor.type - 1 : ULCER_LV_SLEEP;
			smart_bed_display.display_refresh = true;
			break;
		case RIGHT_KEY:		// 교대부양 3개 우 순환 (전시모드는 일반만 → 순환 없음)
			if(!bed_status.reserved && cursor.type >= ULCER_LV_NORMAL)
				cursor.type = (cursor.type < ULCER_LV_SLEEP) ? cursor.type + 1 : ULCER_LV_NORMAL;
			smart_bed_display.display_refresh = true;
			break;
		case CONFORM_KEY:	// 선택 항목 시작 (사용자 맞춤은 설정창 슬라이드업)
			if(cursor.type == ULCER_DISP)			conform_key_proc(CMD2_DISPERSION);
			else if(cursor.type == ULCER_LV_NORMAL)	conform_key_proc(CMD2_VENTIL_STD);
			else if(cursor.type == ULCER_LV_FOCUS)	conform_key_proc(CMD2_VENTIL_FOCUS);
			else if(cursor.type == ULCER_LV_SLEEP){	// 사용자 맞춤 → 설정창 올라옴
				ulcer_config_mode = 1;
				smart_bed_display.display_refresh = true;
			}
			break;
		case SET_KEY:	// 설정 버튼 → 사용자 맞춤 설정창 올라옴
			ulcer_config_mode = 1;
			smart_bed_display.display_refresh = true;
			break;
	}
	remocon_key.key_val = 0xFF;
}

// 3×3 점 그리드 아이콘 (카테고리 라벨 앞) — 스크린샷의 ⋮⋮⋮ 표식.
static void draw_dots_icon(int x, int y, U32 color){
	int r, c;
	for(r = 0; r < 3; r++)
		for(c = 0; c < 3; c++)
			draw_rectfill(x + c * 5, y + r * 5, 2, 2, color);
}

void ulcer_care_draw(void)
{
	static const char* lv_name[3] = {"일반", "집중", "사용자 맞춤"};
	static const char* lv_time[3] = {"5분", "3분", ""};	// 사용자 맞춤: 시간 없음
	bool sel;
	int i, tw;

	set_draw_target(getbackframe());
	draw_rectfill(0, 0, 320, 480, MAKE_COLORREF(20, 25, 38));

	// 헤더 (공용 — 온도/음량/무중력) — 상단 부분은 그대로 유지
	draw_status_header();

	if(bed_status.reserved){
		// ===== [전시버전] 기존 디자인 유지 (체압분산 + 교대부양 일반) =====
		U32 SEL    = MAKE_COLORREF(210, 180, 120);
		U32 UNSEL  = MAKE_COLORREF(90, 85, 70);
		U32 BORDER = MAKE_COLORREF(255, 255, 255);
		draw_title_bar("욕창케어");
		sel = (cursor.type == ULCER_DISP);
		draw_roundrectfill(5, 137, 310, 135, 12, sel ? SEL : UNSEL);
		if(sel) draw_roundrect(5, 137, 310, 135, 12, BORDER);
		egl_font_set_color(g_pFontKor, sel ? MAKE_COLORREF(30, 30, 30) : MAKE_COLORREF(225, 225, 225));
		draw_text_kr(g_pFontKor, 104, 191, "체압분산");
		{
			int by = 286;
			sel = (cursor.type == ULCER_LV_NORMAL);
			draw_roundrectfill(5, by, 310, 135, 12, sel ? SEL : UNSEL);
			if(sel) draw_roundrect(5, by, 310, 135, 12, BORDER);
			egl_font_set_color(g_pFontKor, sel ? MAKE_COLORREF(30, 30, 30) : MAKE_COLORREF(225, 225, 225));
			tw = estimate_text_width_28("교대부양");
			draw_text_kr(g_pFontKor, 5 + (310 - tw) / 2, by + 54, "교대부양");
		}
		draw_home_footer(ulcer_tag_img);
	} else {
		// ===== [일반버전] 새 디자인 — 체압분산 / 교대부양(일반·집중·수면) 세로 리스트 =====
		U32 SELBG = MAKE_COLORREF(255, 255, 255);	// 선택 = 흰색
		U32 SELTC = MAKE_COLORREF(25, 30, 40);
		U32 BLUE  = MAKE_COLORREF(95, 165, 220);	// 비선택 = 파랑
		U32 UNTC  = MAKE_COLORREF(255, 255, 255);
		U32 CATTC = MAKE_COLORREF(175, 180, 190);	// 카테고리 라벨

		const int UDY = 57;
		draw_title_bar("");						// 상단 2번째 줄: 동작 상태(위/아래 구분선)

		if(ulcer_config_anim > 0){
			// 설정창(마사지 config와 동일): 사용자맞춤 박스 리스트위치(361)→상단(160) 상승 + 설정 슬라이드업. 바디만, 헤더·푸터 안 건드림.
			int p = ulcer_config_anim;
			int by = 361 - 201 * p / 100;
			int tw2 = estimate_text_width_28("사용자 맞춤");
			draw_roundrectfill(5, by, 310, 56, 10, SELBG);
			egl_font_set_color(g_pFontKor, SELTC);
			draw_text_kr(g_pFontKor, 5 + (310 - tw2) / 2, by + (56 - 28) / 2, "사용자 맞춤");
			if(p > 10){	// 높이/속도 — 박스 아래에 붙어 박스와 함께 오르내림(마사지처럼)
				static const char* HEIGHT[3] = {"약", "중", "강"};
				static const char* SPEED[3]  = {"느림", "보통", "빠름"};
				int y1 = by + 84, y2 = by + 139;	// 박스(by) 기준 상대 위치 → 같이 이동
				if(y1 < 435) draw_massage_setting_row(y1, "높이", HEIGHT, ulcer_intensity, ulcer_setting_focus == 0);
				if(y2 < 435) draw_massage_setting_row(y2, "속도", SPEED,  ulcer_speed,     ulcer_setting_focus == 1);
			}
		} else {
		// --- 체압분산 ---
		draw_dots_icon(15, 62 + UDY, CATTC);
		egl_font_set_color(g_pFontKor16, CATTC);
		draw_text_kr(g_pFontKor16, 36, 58 + UDY, "체압분산");
		sel = (cursor.type == ULCER_DISP);
		draw_roundrectfill(5, 82 + UDY, 310, 58, 10, sel ? SELBG : BLUE);
		egl_font_set_color(g_pFontKor, sel ? SELTC : UNTC);
		tw = estimate_text_width_28("체압분산");
		draw_text_kr(g_pFontKor, 5 + (310 - tw) / 2, 82 + UDY + (58 - 28) / 2, "체압분산");

		// --- 교대부양 (일반/집중/수면) ---
		draw_dots_icon(15, 160 + UDY, CATTC);
		egl_font_set_color(g_pFontKor16, CATTC);
		draw_text_kr(g_pFontKor16, 36, 156 + UDY, "교대부양");
		for(i = 0; i < 3; i++){
			int by = 180 + UDY + i * 62;	// 동작상태 줄 아래로
			sel = (cursor.type == (U8)(ULCER_LV_NORMAL + i));
			draw_roundrectfill(5, by, 310, 56, 10, sel ? SELBG : BLUE);
			// 모드명 (가운데)
			egl_font_set_color(g_pFontKor, sel ? SELTC : UNTC);
			tw = estimate_text_width_28(lv_name[i]);
			draw_text_kr(g_pFontKor, 5 + (310 - tw) / 2, by + (56 - 28) / 2, lv_name[i]);
			// 동작시간 (우측)
			egl_font_set_color(g_pFontKor16, sel ? MAKE_COLORREF(110, 115, 125) : MAKE_COLORREF(225, 235, 245));
			{
				int dw = kr_text_width_16(lv_time[i]);
				draw_text_kr(g_pFontKor16, 315 - 16 - dw, by + (56 - 16) / 2, lv_time[i]);
			}
		}
		}

		// --- 하단 푸터 (이동 ⇅ / 시작 [확인] / 홈으로 / 욕창케어) — 구분선(440)~바닥(480) 세로 중앙(460) 정렬 ---
		draw_rectfill(5, 440, 310, 1, MAKE_COLORREF(62, 66, 76));	// 하단 구분선 y=440 (HOME과 동일)
		{
			int fcy = 460;					// 푸터 세로 중앙
			int fty = fcy - 8;				// 텍스트 top (16px → 중앙 정렬)
			egl_font_set_color(g_pFontKor16, MAKE_COLORREF(205, 210, 220));
			draw_text_kr(g_pFontKor16, 12, fty, "이동");
			{	// ⇅ 아이콘 — 작게(9) + 위아래 간격(6), 세로 중앙
				U32 ac = MAKE_COLORREF(205, 210, 220);
				int as = 9, agap = 6;
				int atop = fcy - (as * 2 + agap) / 2;
				draw_triangle_up(60, atop, as, ac);
				draw_triangle_down(60, atop + as + agap, as, ac);
			}
			// 가운데: "시작"(사용자 맞춤 포커스 시 "선택") + 확인버튼 이미지 (세로 중앙)
			{
				int sx = 116;
				const char* ctxt = (!bed_status.reserved && cursor.type == ULCER_LV_SLEEP) ? "선택" : "시작";
				draw_text_kr(g_pFontKor16, sx, fty, ctxt);
				if(confirm_btn_img){
					int cx = sx + kr_text_width_16(ctxt) + 5;
					int cy = fcy - confirm_btn_img->h / 2;	// 세로 중앙
					draw_surface(confirm_btn_img, cx, cy);
				}
			}
			// "욕창케어" 이미지(우측) + "홈으로"(그 왼쪽) — 세로 중앙
			if(ulcer_tag_img){
				int ix = 315 - ulcer_tag_img->w;			// 우측 정렬
				int iy = fcy - ulcer_tag_img->h / 2;		// 세로 중앙
				int hw = kr_text_width_16("홈으로");
				draw_surface(ulcer_tag_img, ix, iy);
				draw_text_kr(g_pFontKor16, ix - 12 - hw, fty, "홈으로");
			} else {
				int hw = kr_text_width_16("홈으로");
				int bw2 = kr_text_width_16("욕창케어");
				draw_text_kr(g_pFontKor16, 234 - hw, fty, "홈으로");
				draw_roundrectfill(242, fcy - 12, 72, 25, 8, MAKE_COLORREF(228, 230, 236));
				egl_font_set_color(g_pFontKor16, MAKE_COLORREF(35, 40, 50));
				draw_text_kr(g_pFontKor16, 242 + (72 - bw2) / 2, fty, "욕창케어");
			}
		}
	}

	flip();
}
// ====================== 욕창케어 End ============= //

