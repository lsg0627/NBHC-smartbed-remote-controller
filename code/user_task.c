//////////////////////////////////////////////////////////////////////////////////////////////////
//
// --------- System Timer ----------
// system clock : PCLK(system clock / 2) = 50MHz
// pre-scale : 8192
// time count interval : 8192/50MHz = 164uSec
// 1msec = 164uSec * 6 = 1mSec
// 한글도 되는데
//
//////////////////////////////////////////////////////////////////////////////////////////////////
#include "sdk.h"
#include "../app/main.h"
#include "../drive/smart_bed_remocon.h"


EGL_FONT* g_pFont48;
//EGL_FONT* g_pFont40;
EGL_FONT* g_pFont32;
EGL_FONT* g_pFont28;
EGL_FONT* g_pFont16;
EGL_FONT* g_pFontKor = NULL;
EGL_FONT* g_pFontKor16 = NULL;
//VAIRANCE_ST vairance;
//VAIRANCE_ST vairance_temp;

//U8 bar_status[16] = {0, };


//////////////////////////////////////////////////////////////////////////////////////////////////
//
// System Timer
// Timer Channel : SYS_TIMER_CH
// System Time = (8912/ APB CLK)*DIVIDE
// 1msec = (8912/50MHz)*6
//
//////////////////////////////////////////////////////////////////////////////////////////////////
void sys_timer_set(void){
	// Timer mode, 512 pre-scale, timer enable
	*((volatile unsigned int*)TMCTRL_ADDR(SYS_TIMER_CH))= F_TMCTRL_TMOD_TIMER | F_TMCTRL_PFSEL_8192 | F_TMCTRL_TMEN;
	*((volatile unsigned int*)TPCTRL_ADDR(SYS_TIMER_CH)) = F_TPCTRL_CNTCLR | F_TPCTRL_CLKSEL_SCLK ;// Timer counter reset
	*((volatile unsigned int*)TPCTRL_ADDR(SYS_TIMER_CH)) = F_TPCTRL_CLKSEL_SCLK ;// Timer counter run
}

// timer count
// old_time_cnt : before timer count
// return : 

U16 time_1msec_interval_get(U16 old_timer_cnt){
	U16 cur_timer_cnt;
	
	cur_timer_cnt = *((volatile unsigned int*)TMCNT_ADDR(SYS_TIMER_CH));// get timer count
	if(cur_timer_cnt >= old_timer_cnt){
		return ((cur_timer_cnt - old_timer_cnt) / SYS_TICK_1MS_DIV);
	}
	
	U16 tmp = 0xFFFF - old_timer_cnt;
	return ((tmp + cur_timer_cnt) / SYS_TICK_1MS_DIV);
}

U16 time_10msec_interval_get(U16 old_timer_cnt){
	U16 cur_timer_cnt;
	
	cur_timer_cnt = *((volatile unsigned int*)TMCNT_ADDR(SYS_TIMER_CH));// get timer count
	if(cur_timer_cnt >= old_timer_cnt){
		return ((cur_timer_cnt - old_timer_cnt) / SYS_TICK_10MS_DIV);
	}
	
	U16 tmp = 0xFFFF - old_timer_cnt;
	return ((tmp + cur_timer_cnt) / SYS_TICK_10MS_DIV);
	
}

U16 time_100msec_interval_get(U16 old_timer_cnt)
{
	U16 cur_timer_cnt;

	cur_timer_cnt = *((volatile unsigned int*)TMCNT_ADDR(SYS_TIMER_CH));// get timer count
	if(cur_timer_cnt >= old_timer_cnt){
		return ((cur_timer_cnt - old_timer_cnt) / SYS_TICK_100MS_DIV);
	}

	U16 tmp = 0xFFFF - old_timer_cnt;
	return ((tmp + cur_timer_cnt) / SYS_TICK_100MS_DIV);
	
}

//void progress_10ms_condition()
//{
	//get_bar_run_info();
	//PRINTLINE;
//}
//////////////////////////////////////////////////////////////////////////////////////////////////
//
// target time condition
// 10mSec, 100mSec, 1Sec
//////////////////////////////////////////////////////////////////////////////////////////////////
void process_target_time_handler(void){
	// if(time_1msec_interval_get(pDC->target_time.tmout_10msec) > 10){
		// pDC->target_time.tmout_10msec= *((volatile unsigned int*)TMCNT_ADDR(SYS_TIMER_CH));// get timer count
		// progress_10ms_condition();
	// }
	
	if(time_10msec_interval_get(pDC->target_time.tmout_10msec) > 10){
		pDC->target_time.tmout_10msec = *((volatile unsigned int*)TMCNT_ADDR(SYS_TIMER_CH));// get timer count
		//progress_10ms_condition();
		if(esp32_get_infomation.get_info_time)
			esp32_get_infomation.get_info_time--;
	}
	// if(time_100msec_interval_get(pDC->target_time.tmout_100msec) > 1){
		// pDC->target_time.tmout_100msec = *((volatile unsigned int*)TMCNT_ADDR(SYS_TIMER_CH));// get timer count
		
		// PRINTLINE;
	// }
}

// void progress_10ms_condition(void)
// {
	// PRINTLINE;
	// // if(key_control->read_time > 0) key_control->read_time--;// key read
		
	
	// // if(slave_control->read_tmout>0) slave_control->read_tmout--;					// SERIAL COMMUNICATION
	// // if(WHM_modbus->read_tmout>0)	WHM_modbus->read_tmout--;					// WATT METER COMMUNICATION

	// // if(master_modbus_rtu->read_tmout>0)	master_modbus_rtu->read_tmout--;
	// // if(user_info->t_key_tmout>0) user_info->t_key_tmout--;

// }


SMART_BED_DISP_STATUS smart_bed_display;
SMART_BED_STATUS smart_bed_status;
SURFACE *home_img;     // boot.suf (부팅/초기화 화면)
SURFACE *main_img;     // main.suf (메인 상태 화면 배경)
SURFACE *btm1_img;
SURFACE *btm2_img;
SURFACE *vaira_title_imag;
SURFACE *vaira_main_icon_imag;
SURFACE *vaira_title_imag;
SURFACE *bar_img;
SURFACE *body_imag;
SURFACE *radio_avail_img;
SURFACE *radio_avail_selt_img;
SURFACE *radio_dis_img;
SURFACE *radio_dis_selt_img;
SURFACE *dip_en_num_img;
SURFACE *dip_ds_num_img;

SURFACE *set_title_img;
SURFACE *set_top_manual_sel_img;
SURFACE *set_top_manual_img;
SURFACE *set_top_selftest_sel_img;
SURFACE *set_top_selftest_img;
SURFACE *set_icon_img;

// 교대부양
SURFACE *levitate_title_img;
SURFACE *levitate_main_icon_img;
SURFACE *levitate_normal_icon_img;
SURFACE *levitate_concent_icon_img;
SURFACE *levitate_sleep_icon_img;
SURFACE *levitate_normal_sel_icon_img;
SURFACE *levitate_concent_sel_icon_img;
SURFACE *levitate_sleep_sel_icon_img;

// MASSAGE
SURFACE *massage_title_img;
SURFACE *massage_main_icon_img;
SURFACE *massage_sub_icon_img[12];

// 환자 케어
SURFACE *patient_title_img;
SURFACE *patient_main_icon_img;
SURFACE *patient_head_img;
SURFACE *patient_head_sel_img;
SURFACE *patient_defec_img;
SURFACE *patient_defec_sel_img;
SURFACE *patient_shift_img;
SURFACE *patient_shift_sel_img;

SURFACE *patient_head_main_img;	// 머리감기 이미지
SURFACE *patient_shift_main_img;	// 이동 이미지

SURFACE *heat_titile_img;
SURFACE *heat_mian_icon_img;
SURFACE *heat_on_img;
SURFACE *heat_off_img;

// 통풍
SURFACE *ventil_titile_img ;
SURFACE *ventil_mian_icon_img;
SURFACE *ventil_on_img;
SURFACE *ventil_off_img;

SURFACE *left_arrow_img;
SURFACE *right_arrow_img;
SURFACE *left_run_arrow_img;
SURFACE *right_run_arrow_img;

SURFACE *left_select_img;
SURFACE *right_select_img;

// 자세제어
SURFACE *posture_bg_img;
SURFACE *posture_back_plate_img;
SURFACE *posture_back_plate_a_img;
SURFACE *posture_back_plate_icon_img;
SURFACE *posture_leg_plate_img;
SURFACE *posture_leg_plate_a_img;
SURFACE *posture_leg_plate_icon_img;
SURFACE *posture_all_plate_img;
SURFACE *posture_all_plate_a_img;
SURFACE *posture_all_plate_icon_img;
SURFACE *posture_height_img;
SURFACE *posture_height_a_img;

const char *massage_file_list[12] = {
	"image/top_nav_select.suf",	// 리듬타입
	"image/top_nav_select2.suf", // 물결
	"image/top_nav_select3.suf", // 맥파
 	
	"image/top_nav_select4.suf",
	"image/top_nav_select5.suf",
	"image/top_nav_select6.suf",
	"image/top_nav_select7.suf",
	"image/top_nav_select8.suf",
	"image/top_nav_select9.suf",
	"image/top_nav_select10.suf",
	"image/top_nav_select11.suf",
	"image/top_nav_select12.suf"
};
void image_load(void){
	int i;
	
	home_img = loadsurf("image/boot.suf");
	main_img = loadsurf("image/main.suf");
	btm1_img = loadsurf("image/bottom1.suf");
	btm2_img = loadsurf("image/bottom2.suf");
	vaira_title_imag = loadsurf("image/rc_title_bar_vairance.suf");
	vaira_main_icon_imag = loadsurf("image/main_icon_vairance.suf");
	bar_img = loadsurf("image/bar_active.suf");
	body_imag = loadsurf("image/human.suf");
	
	radio_avail_img = loadsurf("image/radio_available_selected_none.suf");
	radio_avail_selt_img = loadsurf("image/radio_available_selected_focused.suf");
	radio_dis_img = loadsurf("image/radio_disabled_selected_none.suf");
	radio_dis_selt_img = loadsurf("image/radio_disabled_selected_focused.suf");
	dip_en_num_img = loadsurf("image/disp_cnt_bg_available.suf");
	dip_ds_num_img = loadsurf("image/disp_cnt_bg_focused.suf");
	
	set_title_img = loadsurf("image/rc_title_bar_set.suf");
	set_icon_img = loadsurf("image/main_icon_set.suf");
	set_top_manual_sel_img = loadsurf("image/top_manual_btn_focused.suf");
	set_top_manual_img = loadsurf("image/top_manual_btn_default.suf");
	set_top_selftest_sel_img = loadsurf("image/top_self_test_btn_focused.suf");
	set_top_selftest_img = loadsurf("image/top_selft_test_btn_default.suf");
	
	// 교대부양
	levitate_title_img = loadsurf("image/rc_title_bar_levitate.suf");// title
	levitate_main_icon_img = loadsurf("image/main_icon_levitate.suf");// icon 
	
	levitate_normal_icon_img = loadsurf("image/top_normal_btn_default.suf");// 일반
	levitate_concent_icon_img = loadsurf("image/top_concent_btn_default.suf");// 집중
	levitate_sleep_icon_img = loadsurf("image/top_sleep_btn_default.suf");// 수면
	
	levitate_normal_sel_icon_img = loadsurf("image/top_normal_btn_focused.suf");// 일반
	levitate_concent_sel_icon_img = loadsurf("image/top_concent_btn_focused.suf");// 집중
	levitate_sleep_sel_icon_img = loadsurf("image/top_sleep_btn_focused.suf");// 수면
	
	// MASSAGE
	massage_title_img = loadsurf("image/rc_title_bar_massage.suf");// 마사지
	massage_main_icon_img = loadsurf("image/main_icon_massage.suf");// massage icon
	//massage_sub_icon_img[0] = loadsurf("image/top_nav_select.suf");
	for(i = 0; i<12; i++)
		massage_sub_icon_img[i] = loadsurf((char*)massage_file_list[i]);// 리듬타입
	
	// 환자케어
	patient_title_img = loadsurf("image/rc_title_bar_care.suf");
	patient_main_icon_img = loadsurf("image/main_icon_care.suf");
	patient_head_img = loadsurf("image/top_head_wrap_btn_default.suf");
	patient_head_sel_img = loadsurf("image/top_head_wrap_btn_focused.suf");
	patient_defec_img = loadsurf("image/top_meal_btn_default.suf");
	patient_defec_sel_img = loadsurf("image/top_meal_btn_focused.suf");
	patient_shift_img = loadsurf("image/top_shift_btn_default.suf");
	patient_shift_sel_img = loadsurf("image/top_shift_btn_focused.suf");
	
	patient_head_main_img = loadsurf("image/head_wrap.suf");	// 머리감기 이미지
	patient_shift_main_img = loadsurf("image/shift.suf");	// 이동 이미지
	
	heat_titile_img = loadsurf("image/rc_title_bar_heat.suf");	
	heat_mian_icon_img = loadsurf("image/main_icon_heat.suf");	
	heat_on_img = loadsurf("image/hit_on.suf");	
	heat_off_img = loadsurf("image/hit_off.suf");	
	
	ventil_titile_img = loadsurf("image/rc_title_bar_ventilat.suf");	
	ventil_mian_icon_img = loadsurf("image/main_icon_ventilat.suf");	
	ventil_on_img = loadsurf("image/wind_on.suf");	
	ventil_off_img = loadsurf("image/wind_off.suf");	
	
	left_arrow_img = loadsurf("image/btn_left.suf");
	right_arrow_img = loadsurf("image/btn_right.suf");
	left_run_arrow_img = loadsurf("image/btn_left_run.suf");
	right_run_arrow_img = loadsurf("image/btn_right_run.suf");

	left_select_img = loadsurf("image/btn_left_select.suf");
	right_select_img = loadsurf("image/btn_right_select.suf");

	// 자세제어
	posture_bg_img = loadsurf("image/bg.suf");
	posture_back_plate_img = loadsurf("image/back_plate.suf");
	posture_back_plate_a_img = loadsurf("image/back_plate_a.suf");
	posture_back_plate_icon_img = loadsurf("image/back_plate_icon.suf");
	posture_leg_plate_img = loadsurf("image/leg_plate.suf");
	posture_leg_plate_a_img = loadsurf("image/leg_plate_a.suf");
	posture_leg_plate_icon_img = loadsurf("image/leg_plate_icon.suf");
	posture_all_plate_img = loadsurf("image/all_plate.suf");
	posture_all_plate_a_img = loadsurf("image/all_plate_a.suf");
	posture_all_plate_icon_img = loadsurf("image/all_plate_icon.suf");
	posture_height_img = loadsurf("image/height.suf");
	posture_height_a_img = loadsurf("image/height_a.suf");
}

void load_font(void){
	g_pFont28 = create_bmpfont("image/font/font28.fnt");// font load
	bmpfont_setautokerning(g_pFont28, true);// false : 문자간격 고정, true: 문자비율에 맞게 표시
	
	g_pFont16 = create_bmpfont("image/font/font16.fnt");// font load
	bmpfont_setautokerning(g_pFont16, true);// false : 문자간격 고정, true: 문자비율에 맞게 표시

	// 한글 폰트 (SDK 내장 bitfont)
	g_pFontKor = create_bitfont();
	set_bitfontsize(g_pFontKor, 14, 28, 28, 28);	// 영문14x28, 한글28x28

	g_pFontKor16 = create_bitfont();
	set_bitfontsize(g_pFontKor16, 8, 16, 16, 16);	// 영문8x16, 한글16x16
}







// ========================= HOME Start ============================ //
//////////////////////////////////////////////////////////////////////////////////////////////////
// 
// HOME Mode
// 
//////////////////////////////////////////////////////////////////////////////////////////////////
void home_proc(void)
{
	if(smart_bed_display.status != MODE_HOME)
	{
		smart_bed_display.status = MODE_HOME;	// 교대부양 display 갱신
		smart_bed_display.display_refresh = true;// 교대부양 display
		remocon_key.key_val = 0xFF;
		return;
	}

	switch(remocon_key.key_val){
		case CONFORM_KEY:
			// 오버레이 화면에서 확인 버튼 → 동작중↔일시정지 토글
			if(bed_status.run_state == 1) {
				// 동작 중 → 일시정지
				U8 tmp = CMD3_PAUSE;
				esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_PAUSE, &tmp, 1);
				conform_key_run = CMD3_RESTART;
				debugprintf("\n\r HOME: CONFORM -> PAUSE");
			} else if(bed_status.run_state == 2) {
				// 일시정지 → 재시작
				U8 tmp = CMD3_RESTART;
				esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_RESTART, &tmp, 1);
				conform_key_run = CMD3_PAUSE;
				debugprintf("\n\r HOME: CONFORM -> RESUME");
			}
			remocon_key.key_val = 0xFF;
			break;
		case SET_KEY:// 설정/저장 키를 눌렀다.
			smart_bed_status.status = MODE_SET_SAVE;
			remocon_key.key_val = 0xFF;
			break;
	}
}

void home_draw(void){
	set_draw_target(getbackframe());
	if(bed_status.powered_on) {
		// 전원 ON: main.suf 배경 + 상태 오버레이
		// 모드 내 호밍(current_mode != 0 && run_state == 3)도 오버레이 표시
		// 단독 초기화(current_mode == 0 && run_state == 3)는 부팅 화면
		if(bed_status.current_mode == 0x00 && bed_status.run_state == 3) {
			draw_surface(home_img, 0, 0);  // boot.suf: 단독 초기화 중
		} else {
			draw_surface(main_img, 0, 0);
			draw_status_overlay();
		}
	} else {
		// 전원 OFF: boot.suf 배경만
		draw_surface(home_img, 0, 0);
	}
	flip();
}
// ========================= HOME End ============================ //











// ========================= Power On/Off ==========================/
//////////////////////////////////////////////////////////////////////////////////////////////////
//
// Power ON/OFF
// 전원 Off 후 다시 전원 On이 되면 Home Mode
//
// POWER ON  : FF 81 '9''B''1''C' 10 80 00 xx xx
// POWER OFF : FF 81 '9''B''1''C' 10 81 00 xx xx
// CONFIRM   : FF 81 '9''B''1''C' 10 FF 00 xx xx
// STAND BY  : FF 81 '9''B''1''C' 10 F0 00 xx xx
//
// 전원 최초 입력시 키에서 off상태로 LCD는 안나옴
//
//////////////////////////////////////////////////////////////////////////////////////////////////

U8 remo_pwr_st = 0;	// 0(전원 최초 입력), 1(LCD ON), 2(LCD OFF) 0->1<->2

bool stdby_in_progress = false;	// 초기위치 복귀 중 (키 차단)
bool power_off_pending = false;	// ACK 후 전원 OFF 필요
bool stdby_complete = false;	// ESP32 ACK 수신 플래그
U32 stdby_timeout = 0;			// 타임아웃 카운터

void remocon_power_ctrl(U8 remo_pwr)
{
	U8 tmp = 0;

	switch(remo_pwr)
	{
		case REMO_PWR_ON:	// 전원 ON (클릭)
			if(remo_pwr_st == REMO_PWR_ON){
				// 최초 부팅: 초기화 루틴 수행
				esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_PWR_ON, &tmp, 0);
				esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_INIT, &tmp, 0);
				esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_STDBY, &tmp, 0);
			} else {
				// LCD OFF → ON 복귀
				esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_PWR_ON, &tmp, 0);
			}
			remo_pwr_st = REMO_LCD_ON;
			smart_bed_status.status = MODE_HOME;
			smart_bed_display.display_refresh = true;
			stdby_in_progress = false;
			power_off_pending = false;
			LCD_ON();
			break;

		case REMO_LCD_OFF:	// 전원 OFF (ACK 수신 후 또는 직접 호출)
			remo_pwr_st = REMO_LCD_OFF;
			esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_PWR_OFF, &tmp, 0);
			smart_bed_status.status = MODE_HOME;
			running_massage_type = -1;
			stdby_in_progress = false;
			show_shutdown_screen();	// "종료합니다." 표시 후 1.5초 대기
			LCD_OFF();
			break;

		case REMO_STDBY_OFF:	// 롱클릭: 초기위치 복귀 + 전원 OFF 대기
			esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_STDBY, &tmp, 0);
			smart_bed_status.status = MODE_SHUTDOWN;
			smart_bed_display.status = MODE_SHUTDOWN;
			smart_bed_display.display_refresh = true;
			stdby_in_progress = true;
			power_off_pending = true;
			stdby_complete = false;
			stdby_timeout = 6000;	// ~60초 타임아웃 (10ms × 6000)
			break;
	}
}

// 초기위치 복귀 진행 상태 확인 (메인 루프에서 매 사이클 호출)
void check_stdby_progress(void)
{
	if(!stdby_in_progress) return;

	if(stdby_complete){
		debugprintf("\n\r STDBY ACK RECEIVED");
		stdby_in_progress = false;
		stdby_complete = false;

		if(power_off_pending){
			// 롱클릭 → 전원 OFF
			debugprintf("\n\r -> POWER OFF");
			remocon_power_ctrl(REMO_LCD_OFF);
			power = false;
			power_off_pending = false;
		}
		// 숏클릭 → 키 입력 재개 (stdby_in_progress=false로 이미 해제)
		return;
	}

	if(stdby_timeout > 0){
		stdby_timeout--;
	} else {
		// 타임아웃: 강제 해제
		debugprintf("\n\r STDBY TIMEOUT");
		stdby_in_progress = false;
		if(power_off_pending){
			remocon_power_ctrl(REMO_LCD_OFF);
			power = false;
			power_off_pending = false;
		}
	}
}

// bool remocon_power_proc(bool power_on){
	// U8 tmp = 0;
	
	// if(power_on == true){
		// LCD_ON();
		// esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_PWR_ON, &tmp, 0);
		// return true;// power on
	// }
	// LCD_OFF();
	// smart_bed_status.status = MODE_HOME;
	// esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_PWR_OFF, &tmp, 0);
	// return false;// power off
// }

//



void process_analy_data(){
	check_stdby_progress();

	// 낙상 경고 상태: Pause(해제)와 PowerOff만 허용, 나머지 키 무시
	if(smart_bed_status.status == MODE_FALL_ALERT){
		if(remocon_key.key_val == CONFORM_KEY){
			U8 tmp = 0;
			esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_FALL_CLEAR, &tmp, 0);
			smart_bed_status.status = MODE_HOME;
			smart_bed_display.status = MODE_HOME;
			smart_bed_display.display_refresh = true;
			remocon_key.key_val = 0xFF;
			debugprintf("\n\r FALL ALERT DISMISSED by user");
		}
		// PowerOff는 key.c에서 처리됨 (FALL_ALERT 상태에서도 허용)
		remocon_key.key_val = 0xFF;  // 나머지 키 무시
		return;
	}

	switch(smart_bed_status.status){
		case MODE_HOME:
			home_proc();
			break;
		case MODE_SET_SAVE:
			manual_selft_test_proc();
			break;
		case MODE_VAIRANCE:// 체압분산
			dispersion_proc();
			break;
		case MODE_LEVITATE:// 교대부양
			levitate_proc();
			break;
		case MODE_MASSAGE:// 마사지
			massage_proc();
			break;
		case MODE_PATIENT_CARE:// 환자 케어
			patient_care_proc();
			break;
		// 온열/통풍은 key.c에서 직접 처리 (화면 전환 없음)
		// case MODE_SET_SAVE:
			// set_proc();
			// break;
		case MODE_INITIAL:
			initial_proc();
			break;
		case MODE_POSTURE:
			posture_proc();
			break;
	}
}

void progress_lcd_display(void){
	if(smart_bed_display.display_refresh == true){
		switch(smart_bed_display.status){
			case MODE_HOME: // home
				home_draw();
				break;
			//case MODE_INITIAL:// 초기화
			//	break;
			case MODE_SET_SAVE:
				manual_selft_test_draw();
				break;
			case MODE_VAIRANCE:// 체압분산
				dispersion_draw();
				break;
			case MODE_LEVITATE:// 교대부양
				levitate_draw();
				break;
			case MODE_MASSAGE:
				massage_draw();
				break;
			case MODE_PATIENT_CARE:
				patient_care_draw();
				break;
			// 온열/통풍은 화면 없음 (LED만 제어)
			case MODE_INITIAL:
				initial_draw();
				break;
			case MODE_POSTURE:
				posture_draw();
				break;
			case MODE_SHUTDOWN:
				shutdown_draw();
				break;
			case MODE_FALL_ALERT:
				fall_alert_draw();
				break;
		}

		smart_bed_display.display_refresh = false;
	}

}
