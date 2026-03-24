# NINEBELL SMART CARE BED 리모컨 펌웨어 기술 문서

**제품명**: SMART CARE BED Remote Controller
**제조사**: NINEBELL
**SDK**: adStar SDK v3.2 (Advanced Digital Chips Inc.)
**MCU**: adStar (ae32000 EISC Architecture)
**문서 버전**: 1.0
**작성일**: 2026-03-09

---

## 목차

1. [시스템 개요](#1-시스템-개요)
2. [하드웨어 구성](#2-하드웨어-구성)
3. [소프트웨어 구조](#3-소프트웨어-구조)
4. [부팅 프로세스](#4-부팅-프로세스)
5. [키 입력 시스템](#5-키-입력-시스템)
6. [LCD 디스플레이](#6-lcd-디스플레이)
7. [UI 모드 및 상태 관리](#7-ui-모드-및-상태-관리)
8. [ESP32 통신 프로토콜](#8-esp32-통신-프로토콜)
9. [기능별 상세 설명](#9-기능별-상세-설명)
10. [LED 제어](#10-led-제어)
11. [파일시스템 및 이미지 리소스](#11-파일시스템-및-이미지-리소스)
12. [빌드 환경](#12-빌드-환경)
13. [알려진 이슈](#13-알려진-이슈)

---

## 1. 시스템 개요

### 1.1 시스템 구성도

```
┌─────────────────────────────────────────────────────────┐
│                 SMART CARE BED 리모컨                     │
│                                                          │
│  ┌──────────┐    UART1 115200    ┌───────────┐           │
│  │  adStar  │◄─────────────────►│   ESP32    │──► 베드 본체
│  │   MCU    │    (SB1C 프로토콜)  │ (통신모듈)  │           │
│  │(ae32000) │                    └───────────┘           │
│  └──┬──┬──┬─┘                                            │
│     │  │  │                                              │
│  ┌──┴┐┌┴──┐┌┴──────┐  ┌────────┐  ┌──────┐  ┌────────┐  │
│  │LCD││SPI││NAND   │  │ LED x6 │  │ USB  │  │UART0   │  │
│  │ILI││KEY││Flash  │  │온열 x3 │  │ Host │  │Debug   │  │
│  │948││입력││(FAT)  │  │통풍 x3 │  │      │  │115200  │  │
│  │8  ││   ││       │  │        │  │      │  │        │  │
│  │320││시프││이미지  │  │P3.0~4  │  │P8.1~2│  │P1.0    │  │
│  │x  ││트 ││폰트   │  │P4.4    │  │      │  │        │  │
│  │480││레지││boot   │  │        │  │      │  │        │  │
│  │   ││스터││.bin   │  │        │  │      │  │        │  │
│  └───┘└───┘└───────┘  └────────┘  └──────┘  └────────┘  │
└─────────────────────────────────────────────────────────┘
```

### 1.2 리모컨 외관 및 버튼 배치

```
    ┌──────────────────────────┐
    │  (⏻)  SMART CARE BED    │
    │                          │
    │  ┌────────────────────┐  │
    │  │                    │  │
    │  │    320 x 480 LCD   │  │
    │  │    (ILI9488)       │  │
    │  │                    │  │
    │  └────────────────────┘  │
    │                          │
    │  [체압분산]    [교대부양]  │
    │                          │
    │           (↑)            │
    │     (←) [확인] (→)       │
    │        START/STOP        │
    │           (↓)            │
    │                          │
    │  [마사지]      [돌봄케어]  │
    │                          │
    │  [설정/저장]    [초기화]   │
    │                          │
    │  [온열]        [통풍]     │
    │  ● ● ●        ● ● ●     │
    │                          │
    │        NINEBELL           │
    └──────────────────────────┘
```

---

## 2. 하드웨어 구성

### 2.1 MCU 사양

| 항목 | 사양 |
|------|------|
| MCU | adStar (ae32000 EISC) |
| 시스템 클럭 | 100MHz (PCLK = 50MHz) |
| ROM | 512KB (0x00000000) |
| RAM | 7,936KB (0x20800000) |
| Flash | NOR Flash + NAND Flash |

### 2.2 GPIO 핀 맵

#### Port 0 (P0) - 부트 모드 / 범용

| 핀 | 방향 | 기능 | 비고 |
|----|------|------|------|
| P0.0 | IN | GPIO | 미사용 |
| P0.1 | IN | GPIO | 미사용 |
| P0.2 | IN | GPIO | 부트 모드 선택 비트 0 |
| P0.3 | IN | GPIO | 부트 모드 선택 비트 1 |
| P0.4 | - | SFLASH_CS | Serial Flash |
| P0.5 | - | SFLASH_DQ1 | Serial Flash |
| P0.6 | - | SFLASH_DQ2 | Serial Flash |
| P0.7 | IN | GPIO | 미사용 |

#### Port 1 (P1) - NAND / Debug UART

| 핀 | 방향 | 기능 | 비고 |
|----|------|------|------|
| P1.0 | OUT | UART_TX0 | 디버그 시리얼 출력 |
| P1.1 | OUT | GPIO | 미사용 |
| P1.2 | - | NF_CS | NAND Flash CS |
| P1.3 | - | NF_ALE | NAND Flash ALE |
| P1.4 | - | NF_CLE | NAND Flash CLE |
| P1.5 | - | NF_WE | NAND Flash WE |
| P1.6 | - | NF_RE | NAND Flash RE |
| P1.7 | IN | NF_BUSYX | NAND Flash Busy |

#### Port 2 (P2) - NAND 데이터 버스

| 핀 | 방향 | 기능 |
|----|------|------|
| P2.0~7 | I/O | NAND Data[0:7] |

#### Port 3 (P3) - LED / LCD 백라이트 / 키 입력

| 핀 | 방향 | 기능 | 비고 |
|----|------|------|------|
| P3.0 | OUT | LED | 온열 LED 3 |
| P3.1 | OUT | LED | 온열 LED 2 |
| P3.2 | OUT | LED | 온열 LED 1 |
| P3.3 | OUT | LED | 통풍 LED 3 |
| P3.4 | OUT | LED | 통풍 LED 2 |
| P3.5 | OUT | GPIO | LCD 백라이트 ON/OFF |
| P3.6 | OUT | GPIO | 키 시프트 레지스터 1 LATCH |
| P3.7 | OUT | GPIO | 키 시프트 레지스터 1 CS |

#### Port 4 (P4) - ESP32 UART / SPI / 키 입력

| 핀 | 방향 | 기능 | 비고 |
|----|------|------|------|
| P4.0 | OUT | GPIO | 키 시프트 레지스터 0 LATCH |
| P4.1 | OUT | GPIO | 키 시프트 레지스터 0 CS |
| P4.2 | OUT | UART_TX1 | ESP32 송신 |
| P4.3 | IN | UART_RX1 | ESP32 수신 |
| P4.4 | OUT | LED | 통풍 LED 1 |
| P4.5 | OUT | GPIO | 미사용 |
| P4.6 | - | SPI_SCK1 | LCD / 키 SPI 클럭 |
| P4.7 | OUT | GPIO | LCD SPI CS |

#### Port 5 (P5) - LCD 인터페이스

| 핀 | 방향 | 기능 | 비고 |
|----|------|------|------|
| P5.0 | IN | SPI_MISO1 | 키 SPI 데이터 입력 |
| P5.1 | OUT | SPI_MOSI1 | LCD SPI 데이터 출력 |
| P5.2 | OUT | GPIO | LCD RS (CMD/DATA 선택) |
| P5.3 | OUT | GPIO | LCD RESET |
| P5.4 | - | VSYNC | LCD 수직 동기 |
| P5.5 | - | HSYNC | LCD 수평 동기 |
| P5.6 | - | DISP_EN | LCD 디스플레이 Enable |
| P5.7 | - | CRTC_CLK_OUT | LCD 도트 클럭 |

#### Port 6, 7 (P6, P7) - LCD RGB 데이터

| 포트 | 기능 |
|------|------|
| P6.0~7 | RED[7:0] |
| P7.3~7 | GREEN[7:3] |

#### Port 8 (P8) - LCD Blue / USB

| 핀 | 방향 | 기능 | 비고 |
|----|------|------|------|
| P8.0 | OUT | GPIO | 미사용 |
| P8.1 | IN | GPIO | USB 연결 감지 (LOW=감지) |
| P8.2 | OUT | GPIO | USB Enable/Disable |
| P8.3~7 | - | B[3:7] | LCD BLUE 데이터 |

#### Port 9 (P9) - Serial Flash

| 핀 | 기능 |
|----|------|
| P9.0 | SFLASH_DQ0 |
| P9.1 | SFLASH_CLK |
| P9.2 | SFLASH_DQ3 |

### 2.3 부트 모드 설정

GPIO P0.2, P0.3 조합으로 부트 모드 결정:

| P0.3 | P0.2 | 모드값 | 동작 |
|------|------|--------|------|
| 1 | 1 | 0x0C | NAND Flash 부팅 (일반 동작) |
| 1 | 0 | 0x08 | Developer Mode (USB 펌웨어 다운로드) |
| 0 | 1 | 0x04 | NOR Flash 부팅 |

---

## 3. 소프트웨어 구조

### 3.1 디렉터리 구조

```
smart_bed/
├── app/                          # 애플리케이션 진입점
│   ├── main.c                    # main() 함수, 메인 루프
│   ├── main.h                    # 전체 헤더 통합 (#include 모음)
│   └── Makefile.mk               # 앱 빌드 설정
│
├── boot_loader/                  # 부트로더
│   ├── main.c                    # 부트로더 진입점 (부팅 모드 분기)
│   ├── dwn.png                   # Developer 모드 부팅 이미지 (320x480)
│   ├── mass.png                  # Mass Storage 모드 부팅 이미지 (320x480)
│   ├── Makefile.mk               # 부트로더 빌드 설정
│   └── adstar-bootloader.ld      # 부트로더 링커 스크립트
│
├── code/                         # 애플리케이션 로직
│   ├── user_task.c               # UI 로직 (모드별 proc/draw), 이미지 로드
│   └── ui_globals.c              # 전역 변수 초기화 (init_value)
│
├── drive/                        # 하드웨어 드라이버
│   ├── smart_bed_remocon.c/h     # GPIO 초기화, 모드 데이터, 커서/키 처리
│   ├── lcd.c/h                   # ILI9488 LCD 드라이버 (SPI 제어)
│   ├── key.c/h                   # SPI 키 입력 (시프트 레지스터)
│   └── protocol.c/h              # ESP32 UART 통신 프로토콜
│
├── include/                      # 공용 헤더
│   ├── display.h                 # UI 좌표 상수 (중복, lcd.h에도 존재)
│   ├── user_task.h               # 모드 enum, SURFACE 이미지 extern 선언
│   └── ui_globals.h              # 디바이스 제어 구조체
│
└── application_250413_R00/       # 이전 버전 애플리케이션 (참고용)
```

### 3.2 소스 파일별 역할

| 파일 | 줄 수 | 주요 역할 |
|------|-------|-----------|
| `app/main.c` | 80 | 시스템 초기화, 메인 이벤트 루프 |
| `code/user_task.c` | ~540 | 이미지 로드, 타이머, 모드별 UI proc/draw, 전원 제어, 종료 진행 관리 |
| `code/ui_globals.c` | 17 | pDC 초기화, 상태값 초기화 |
| `drive/smart_bed_remocon.c` | ~1460 | 포트 초기화, LED 제어, 커서/키 처리, 모드별 초기값, conform 처리 |
| `drive/lcd.c` | 125 | SPI 초기화, ILI9488 LCD 초기화 명령 |
| `drive/key.c` | ~310 | SPI 시프트 레지스터 키 읽기, 키 디바운싱, 전원키 롱/숏클릭 판별, 키 이벤트 분배 |
| `drive/protocol.c` | 635 | ESP32 UART 통신 (패킷 송수신, CRC16, 파싱) |

### 3.3 주요 자료구조

```c
// 모드 상태 열거형
typedef enum {
    MODE_HOME = 0,        // 홈 화면
    MODE_VAIRANCE,        // 체압분산
    MODE_LEVITATE,        // 교대부양
    MODE_MASSAGE,         // 마사지
    MODE_PATIENT_CARE,    // 돌봄케어
    MODE_HEAT,            // 온열
    MODE_VENTILATION,     // 통풍
    MODE_SET_SAVE,        // 설정/저장
    MODE_INITIAL,         // 초기화
    MODE_SHUTDOWN,        // 종료 화면 ("종료합니다.")
    MODE_MAX
} _status;

// 디스플레이 상태
typedef struct {
    U8 status;              // 현재 표시 모드
    bool display_refresh;   // 화면 갱신 플래그
} SMART_BED_DISP_STATUS;

// 신체 부위별 설정 데이터
typedef struct {
    U8 body[BODY_MAX][BODY_LEVIT_MAX+1];
    // body[부위][0] = Enable/ID (0x10=머리, 0x20=상체, 0x30=하체, 0x40=다리)
    // body[부위][1] = 시간 (0x10 단위)
    // body[부위][2] = 높이 (0x10 단위)
    // body[부위][3] = RPM/속도 (0x10 단위)
} BODY;

// 커서 (UI 네비게이션)
typedef struct {
    U8 mode;       // CURSOR_MODE / CURSOR_BODY / CURSOR_SET
    U8 type;       // 서브 타입 (일반/집중/수면 등)
    U8 type_max;   // 서브 타입 최대값
    U8 body;       // 신체 부위 (머리/상체/하체/다리)
    U8 set;        // 설정 항목 (시간/높이/속도)
    U8 set_max;    // 설정 항목 최대값
} CURSOR;

// ESP32 통신 구조체
typedef struct {
    bool rx_flag;               // 수신 중 플래그
    int rx_read_pointer;        // 수신 버퍼 읽기 포인터
    int rx_write_pointer;       // 수신 버퍼 쓰기 포인터
    U8 packet_status;           // 패킷 상태
    U16 rx_time_count;          // 수신 타임아웃 카운터
    U8 rx_buff[2048];           // 수신 링 버퍼 (2KB)
    U8 tx_buff[1024];           // 송신 버퍼 (1KB)
    U8 pt_buff[2048];           // 파싱 버퍼
    int pt_cnt;                 // 파싱 카운트
    U16 crc16;                  // CRC16 값
    int data_leng;              // 데이터 길이
    U8 comm_status;             // 통신 상태
} ESP32_COMM;
```

---

## 4. 부팅 프로세스

### 4.1 부트로더 흐름

```
전원 인가
    │
    ▼
boot_loader/main.c : main()
    │
    ├── PNG 이미지 로드 (INCBIN으로 바이너리에 포함)
    │   ├── dwn.png → dwn_image (SURFACE*)
    │   └── mass.png → mass_image (SURFACE*)
    │
    ├── smart_bed_remocon_port_init()   ← GPIO 포트 초기화
    ├── uart_config(CH0, 115200)        ← 디버그 UART
    ├── get_smart_bed_remocon_boot_mode() ← P0.2~3 읽기
    │
    ├── USB 감지 확인 (usb_get_detection)
    │   │
    │   ├── USB 감지 + DEVELOPER_MODE (0x08)
    │   │   ├── LCD 초기화 → dwn.png 표시
    │   │   └── RSP_Run() → 펌웨어 다운로드 모드
    │   │
    │   └── USB 감지 + 기타
    │       ├── LCD 초기화 → mass.png 표시
    │       └── mass_storage_main() → USB 대용량 저장장치 모드
    │
    └── USB 미감지 (정상 부팅)
        │
        ├── NAND_FLASH_MODE (0x0C)
        │   └── bin_execute_fat()
        │       ├── f_mount("0:", NAND)
        │       ├── f_open("boot.bin")     ← NAND FAT에서 앱 바이너리 로드
        │       ├── f_read() → 0x20000000  ← RAM에 복사
        │       └── entryfp()              ← 앱 진입점 실행
        │
        └── NOR_FLASH_MODE (0x04)
            └── bin_execute()
                ├── memcpy(0x20000000, 0x14000, ...) ← NOR Flash→RAM 복사
                └── entryfp()              ← 앱 진입점 실행
```

### 4.2 애플리케이션 초기화 흐름

```
app/main.c : main()
    │
    ├── smart_bed_remocon_port_init()     # GPIO 포트 재설정
    ├── uart_config(CH0, 115200)          # 디버그 UART
    │
    ├── SPI_init()                        # SPI CH1 마스터 (500kHz)
    ├── LCD_Init()                        # ILI9488 초기화 (SPI 커맨드)
    ├── crtc_clock_init()                 # 12MHz 픽셀 클럭
    ├── key_init()                        # 키 시프트 레지스터 초기화
    │
    ├── f_mount("0:", NAND, 1)            # NAND 파일시스템 마운트
    │
    ├── createframe(320,480,16) x2        # 더블 프레임 버퍼 생성
    ├── setscreenex(320,480,RGB565,...)   # HVSYNC 60Hz 타이밍 설정
    ├── setdoubleframebuffer()            # 더블 버퍼링 활성화
    │
    ├── image_load()                      # NAND에서 .suf 이미지 로드 (50+개)
    ├── load_font()                       # 비트맵 폰트 로드 (28pt, 16pt)
    │
    ├── esp32_com_init()                  # ESP32 UART1 (115200bps) 초기화
    ├── init_value()                      # 상태 변수 초기화 (display=0xFF)
    ├── *_value_power_init() x6           # 각 모드 기본값 설정
    ├── esp32_get_info_init()             # ESP32 정보 요청 타이머 초기화
    ├── sys_timer_set()                   # 시스템 타이머 설정
    │
    └── while(1) {                        # ===== 메인 루프 =====
          process_target_time_handler()   # 10ms 타이머 이벤트
          key_read()                      # 키 입력 처리
          process_analy_data()            # 모드별 로직 처리
          esp32_get_info_bar_body()       # ESP32 수신 데이터 처리
          progress_lcd_display()          # 화면 갱신
        }
```

---

## 5. 키 입력 시스템

### 5.1 하드웨어 구성

SPI 인터페이스를 통해 2개의 8비트 시프트 레지스터에서 키 상태를 읽음.

```
시프트 레지스터 0 (8비트)          시프트 레지스터 1 (8비트)
├── LATCH: P4.0                   ├── LATCH: P3.6
├── CS:    P4.1                   ├── CS:    P3.7
├── DATA:  SPI MISO (P5.0)       ├── DATA:  SPI MISO (P5.0)
└── CLK:   SPI SCK1 (P4.6)       └── CLK:   SPI SCK1 (P4.6)

결과: key_value = (key1 << 8) | key0  →  16비트 키 상태
```

### 5.2 키 읽기 프로세스

```
get_spi_key()
    │
    ├── LATCH0 LOW → HIGH (200μs + 500μs)     # 레지스터 0 래치
    ├── CS0 LOW → SPI 전송 0xFF → key0 읽기    # 레지스터 0 데이터
    ├── CS0 HIGH
    │
    ├── LATCH1 LOW → HIGH (200μs + 500μs)     # 레지스터 1 래치
    ├── CS1 LOW → SPI 전송 0xFF → key1 읽기    # 레지스터 1 데이터
    ├── CS1 HIGH
    │
    └── return (key1 << 8) | key0              # 16비트 합성
```

### 5.3 키 비트 매핑 (Active Low)

| 비트 | 매크로 | 리모컨 버튼 | 설명 |
|------|--------|-------------|------|
| 0 | `POWER_KEY` | ⏻ 전원 | 전원 ON/OFF 토글 |
| 1 | `VAIRANCE_KEY` | 체압분산 | MODE_VAIRANCE 진입 |
| 2 | `LEVITATE_KEY` | 교대부양 | MODE_LEVITATE 진입 |
| 3 | `MASSA_KEY` | 마사지 | MODE_MASSAGE 진입 |
| 4 | `CARE_KEY` | 돌봄케어 | MODE_PATIENT_CARE 진입 |
| 5 | `CONFORM_KEY` | 확인/START/STOP | 설정 확인, 동작 시작/정지 |
| 6 | `UP_KEY` | ↑ | 신체 부위/설정 항목 이동 |
| 7 | - | (미사용) | - |
| 8 | `DOWN_KEY` | ↓ | 신체 부위/설정 항목 이동 |
| 9 | `LEFT_KEY` | ← | 서브 타입 이동 / 값 감소 |
| 10 | `RIGHT_KEY` | → | 서브 타입 이동 / 값 증가 |
| 11 | `HEAT_KEY` | 온열 | MODE_HEAT 진입 |
| 12 | `VENTIL_KEY` | 통풍 | MODE_VENTILATION 진입 |
| 13 | `SET_KEY` | 설정/저장 | MODE_SET_SAVE 진입 |
| 14 | `INIT_KEY` | 초기화 | MODE_INITIAL 진입 |

`KEY_MASK = 0x7F7F` (bit 7, bit 15 제외)

### 5.4 디바운싱

```c
#define KEY_CNT      5    // 디바운싱 카운트 (약 0.1초)
#define LONG_KEY_CNT 124  // 롱클릭 판정 카운트 (약 3초)

key_proc() {
    current = get_spi_key() | ~KEY_MASK;  // 미사용 비트(7,15) 노이즈 차단
    if (old != current) {
        old = current;
        count = KEY_CNT;     // 카운트 리셋
        pushed = false;
        return;
    }
    if (count > 0) {
        count--;              // 카운트 감소
        return;
    }
    if (all_released) return; // 모든 키 해제 상태
    if (!pushed && !run) {
        pushed = true;        // 유효 키 입력 확정
    }
}
```

### 5.5 키 이벤트 처리 흐름

```
key_read()
    │
    ├── [전원 ON 상태] 전원키 숏/롱 판별 (독립 처리)
    │   ├── 전원키 누름 감지 → 추적 시작 (power_key_active = true)
    │   ├── 독립 홀드 카운터 (power_hold_cnt): SPI 원시값 기반
    │   │   └── power_hold_cnt >= LONG_KEY_CNT(124) → 즉시 종료 루틴
    │   │       ├── remocon_power_ctrl(REMO_STDBY_OFF)
    │   │       ├── "종료합니다." 화면 표시 (MODE_SHUTDOWN)
    │   │       └── power = false, 모든 키 차단
    │   └── 릴리즈 디바운스 (노이즈 내성: 감소 방식)
    │       └── power_release_cnt >= KEY_CNT(5) → 숏클릭 확정
    │           ├── STDBY HOME (초기위치 복귀 + 홈 화면)
    │           └── stdby_in_progress = true (키 차단)
    │
    ├── [stdby_in_progress] → return  // 초기위치 복귀 중 모든 키 차단
    │
    ├── [전원 OFF 상태] POWER_KEY → remocon_power_ctrl(REMO_PWR_ON)
    │                               power = true
    │
    ├── CONFORM_KEY → key_val = CONFORM_KEY         // 확인 키
    ├── UP_KEY      → key_val = UP_KEY              // 상 이동
    ├── DOWN_KEY    → key_val = DOWN_KEY            // 하 이동
    ├── LEFT_KEY    → key_val = LEFT_KEY            // 좌 이동/값 감소
    ├── RIGHT_KEY   → key_val = RIGHT_KEY           // 우 이동/값 증가
    ├── VAIRANCE_KEY → status = MODE_VAIRANCE       // 체압분산
    ├── LEVITATE_KEY → status = MODE_LEVITATE       // 교대부양
    ├── MASSA_KEY    → status = MODE_MASSAGE        // 마사지
    ├── CARE_KEY     → status = MODE_PATIENT_CARE   // 돌봄케어
    ├── HEAT_KEY     → status = MODE_HEAT           // 온열
    ├── VENTIL_KEY   → status = MODE_VENTILATION    // 통풍
    ├── SET_KEY      → key_val = SET_KEY            // 설정/저장
    └── INIT_KEY     → status = MODE_INITIAL        // 초기화
```

### 5.6 전원키 노이즈 대책

SPI 시프트 레지스터의 미사용 비트(7, 15)가 플로팅되어 노이즈 발생 가능. 이를 방지하기 위한 3가지 대책:

1. **SPI 미사용 비트 마스킹**: `get_spi_key() | ~KEY_MASK`로 미사용 비트를 항상 1로 고정
2. **독립 홀드 카운터**: `key_proc()`의 `hold_count`가 바운스로 리셋되는 문제 회피. SPI 원시값 기반 `power_hold_cnt`로 독립 추적
3. **노이즈 내성 릴리즈 디바운스**: 릴리즈 카운터를 리셋(=0) 대신 감소(-1)하여 간헐적 노이즈 허용. 안전 타임아웃으로 `power_active_frames > (LONG_KEY_CNT + KEY_CNT) * 2` 시 강제 릴리즈 확정

---

## 6. LCD 디스플레이

### 6.1 LCD 사양

| 항목 | 값 |
|------|-----|
| 드라이버 IC | ILI9488 |
| 해상도 | 320 x 480 |
| 색상 포맷 | RGB565 (16bit) |
| 제어 인터페이스 | SPI (4-wire, 500kHz) |
| 그래픽 인터페이스 | RGB 패러렐 (HVSYNC) |
| 프레임 레이트 | 60Hz (HVSYNC), 70Hz (LCD 내부) |
| 픽셀 클럭 | 12MHz |
| 백라이트 제어 | P3.5 GPIO (ON/OFF) |

### 6.2 LCD 초기화 시퀀스

```
RESET LOW (200ms) → RESET HIGH (100ms)
    │
    ├── 0xC0: 전원 제어 1 (VREG1=0x18, VREG2=0x16)
    ├── 0xC1: 전원 제어 2 (VGH/VGL 설정)
    ├── 0xC5: VCOM 제어
    ├── 0x36: 메모리 접근 제어 (스캔 방향)
    ├── 0x3A: 인터페이스 픽셀 포맷 → 0x55 (RGB565 16bit)
    ├── 0xB0: 인터페이스 모드 제어
    ├── 0xB1: 프레임 레이트 → 0xA0 (70Hz)
    ├── 0xB4: 디스플레이 반전 제어
    ├── 0xB6: RGB/MCU 인터페이스 → 0xB2 (RGB 모드)
    ├── 0xE9: 세트 이미지 기능
    ├── 0xF7: 어드저스트 제어 3
    ├── 0x11: Sleep Out (50ms 대기)
    ├── 0x13: Normal Display Mode ON
    └── 0x29: Display ON (120ms 대기)
```

### 6.3 그래픽 시스템

```
더블 프레임 버퍼:
    frame  (320x480, RGB565) ─┐
    frame2 (320x480, RGB565) ─┤
                               ▼
    setdoubleframebuffer(frame, frame2)

그리기 과정:
    set_draw_target(getbackframe())  ← 후면 버퍼에 그리기
    draw_surface() / draw_rectfill() / bmpfont_draw() 등
    flip()                           ← 전면/후면 버퍼 교체 (화면 표시)

CRTC 타이밍 (HVSYNC 60Hz @ 12MHz):
    H: Total=400, Sync Start=3, Sync End=16, Active=320+16
    V: Total=500, Sync Start=3, Sync End=15, Active=480+15
```

### 6.4 그리기 함수 목록

| 함수 | 설명 |
|------|------|
| `draw_surface(surf, x, y)` | SURFACE 이미지 그리기 |
| `draw_rectfill(x, y, w, h, color)` | 채워진 사각형 |
| `draw_rect(x, y, w, h, color)` | 사각형 테두리 |
| `draw_line(x1, y1, x2, y2, color)` | 직선 |
| `draw_circlefill(x, y, r, color)` | 채워진 원 |
| `bmpfont_draw(font, x, y, str)` | 비트맵 폰트 텍스트 |
| `egl_font_set_color(font, color)` | 폰트 색상 설정 |
| `MAKE_COLORREF(r, g, b)` | RGB 색상 생성 |
| `loadsurf(filename)` | .suf 파일 로드 (프리디코딩된 RGB565) |
| `loadpng(filename)` | .png 파일 로드 |
| `loadjpg(filename)` | .jpg 파일 로드 |
| `loadbmp(filename)` | .bmp 파일 로드 |

---

## 7. UI 모드 및 상태 관리

### 7.1 상태 머신

```
                         ┌──────────────┐
             전원 ON ───►│  MODE_HOME   │◄─── 전원 재ON / 초기화 완료
                         │  (홈 화면)    │
                         └──────┬───────┘
                                │ 기능 키 입력
    ┌──────┬──────┬──────┬──────┼──────┬──────┬──────┐
    ▼      ▼      ▼      ▼      ▼      ▼      ▼      ▼
  체압    교대    마사    돌봄    온열    통풍    설정    초기화
  분산    부양    지      케어                   저장
  (1)     (2)    (3)     (4)    (5)    (6)     (7)    (8)
    │      │      │       │      │      │       │      │
    └──────┴──────┴───────┴──────┴──────┴───────┴──────┘
              각 모드에서 다른 기능 키로 직접 전환 가능

  ※ 전원키 롱클릭(~3초) 시:
    [任意 모드] ──롱클릭──▶ MODE_SHUTDOWN ──ESP32 ACK──▶ LCD OFF
                          ("종료합니다." 표시)
                          (모든 키 입력 차단)
```

### 7.2 각 모드의 proc/draw 구성

모든 모드는 동일한 패턴으로 구현:

```c
void xxx_proc() {
    if (display.status != MODE_XXX) {
        // 최초 진입: 상태 초기화
        display.status = MODE_XXX;
        display.display_refresh = true;
        // 커서 초기화, 임시 데이터 복사
        return;
    }
    switch (remocon_key.key_val) {
        // 키 입력별 동작 처리
    }
}

void xxx_draw() {
    set_draw_target(getbackframe());
    // 타이틀 바, 메인 아이콘, 바디 이미지, 라디오 버튼, 수치 표시 등
    flip();
}
```

### 7.3 UI 레이아웃 좌표

```
(0,0)───────────────────────(320,0)
│  HOME_ICON (15,15)                │
│           TITLE (65,0)            │
│                                   │
│      MAIN_ICON (115,50)           │
│                                   │
│  SUB_TYPE 버튼 (21~, 148)        │
│                                   │
│  LABLE0 (232,210)                 │
│  BAR (95,214)   BODY (116,216)    │
│  RADIO (24,228) ──── 머리         │
│  RADIO (24,278) ──── 상체(상)     │
│  LABLE1 (232,292)                 │
│  RADIO (24,327) ──── 상체(하)     │
│  LABLE2 (232,374)                 │
│  RADIO (24,408) ──── 다리         │
│                                   │
│  BOTTOM BAR                       │
(0,480)─────────────────────(320,480)
```

### 7.4 커서 네비게이션

3단계 계층 구조:

```
CURSOR_MODE (타입 선택)
    ← → : 서브 타입 전환 (일반/집중/수면 등)
    설정/저장 키 : CURSOR_BODY로 전환
         │
         ▼
CURSOR_BODY (신체 부위 선택)
    ↑ ↓ : 머리/상체(상)/상체(하)/다리 이동
    ← → : CURSOR_SET으로 전환
    설정/저장 키 : CURSOR_MODE로 복귀 (값 저장)
         │
         ▼
CURSOR_SET (설정값 조절)
    ↑ ↓ : 시간/높이/속도 항목 이동
    ← : 값 감소 (0x10 단위)
    → : 값 증가 (0x10 단위, 최대값 제한)
```

### 7.5 화면 갱신 메커니즘

```c
// 메인 루프에서 호출
void progress_lcd_display() {
    if (display_refresh == true) {
        switch (display.status) {
            case MODE_HOME:       home_draw();              break;
            case MODE_VAIRANCE:   dispersion_draw();        break;
            case MODE_LEVITATE:   levitate_draw();          break;
            case MODE_MASSAGE:    massage_draw();           break;
            case MODE_PATIENT_CARE: patient_care_draw();    break;
            case MODE_HEAT:       heat_draw();              break;
            case MODE_VENTILATION: ventilation_draw();      break;
            case MODE_SET_SAVE:   manual_selft_test_draw(); break;
            case MODE_INITIAL:    initial_draw();           break;
            case MODE_SHUTDOWN:   shutdown_draw();          break;
        }
        display_refresh = false;  // 갱신 완료
    }
}
```

- `display_refresh`가 `true`일 때만 화면을 다시 그림 (불필요한 리드로잉 방지)
- 키 입력, 데이터 수신 등의 이벤트에서 `display_refresh = true`로 설정

---

## 8. ESP32 통신 프로토콜

### 8.1 통신 사양

| 항목 | 값 |
|------|-----|
| UART 채널 | CH1 |
| 보율 | 115,200 bps |
| 데이터 비트 | 8 |
| 패리티 | None |
| 스톱 비트 | 1 |
| 흐름 제어 | None |
| 바이트 순서 | Little Endian |

### 8.2 패킷 구조

```
┌──────────┬──────────────┬──────┬──────┬────────┬──────────┬─────────┐
│   STX    │  PROD CODE   │ CMD1 │ CMD2 │ LENGTH │ DATA(N)  │  CRC16  │
│  0xFF81  │  "SB1C"      │  1B  │  1B  │   1B   │  N Byte  │   2B    │
├──────────┼──────────────┼──────┼──────┼────────┼──────────┼─────────┤
│ Byte 0-1 │  Byte 2-5    │  6   │  7   │   8    │  9~(8+N) │(9+N)~   │
└──────────┴──────────────┴──────┴──────┴────────┴──────────┴─────────┘

총 패킷 크기 = 11 + N (데이터 길이)
```

| 필드 | 크기 | 값 | 설명 |
|------|------|-----|------|
| STX | 2B | 0xFF, 0x81 | 패킷 시작 |
| PROD CODE | 4B | 'S','B','1','C' | 제품 코드 |
| CMD1 | 1B | - | 명령 분류 |
| CMD2 | 1B | - | 세부 동작 |
| LENGTH | 1B | 0~255 | 데이터 바이트 수 |
| DATA | NB | - | 가변 데이터 |
| CRC16 | 2B | - | CRC-16 체크섬 |

### 8.3 CRC-16 계산

```
알고리즘: CRC-16/Modbus (반전)
초기값: 0xFFFF
다항식: 0xA001 (반사 다항식, 원래 0x8005)
입력 범위: STX ~ DATA (LENGTH+1 바이트까지)
결과: 상위/하위 바이트 스왑 후 저장
```

### 8.4 명령 체계

#### CMD1: 명령 분류

| CMD1 | 방향 | 설명 |
|------|------|------|
| `0x10` | 리모컨 → 베드 | 동작 상태 전송 (실행/정지) |
| `0x20` | 리모컨 → 베드 | 설정값 전송 (시간, RPM, 높이 등) |
| `0x30` | 리모컨 → 베드 | 바(에어셀) 정보 요청 |
| `0x50` | 베드 → 리모컨 | 바(에어셀) 정보 응답 |
| `0x70` | 베드 → 리모컨 | 신체 감지 정보 응답 |

#### CMD2: 동작 상태 전송 (CMD1=0x10)

| CMD2 | 설명 | DATA |
|------|------|------|
| `0x80` | 전원 ON | - |
| `0x81` | 전원 OFF | - |
| `0xFF` | 초기화 확인 | - |
| `0xF0` | 초기화(스탠바이) 실행 | - |
| `0x10` | 교대부양 - 일반 모드 | 신체 부위별 설정값 |
| `0x11` | 교대부양 - 집중 모드 | 신체 부위별 설정값 |
| `0x12` | 교대부양 - 수면 모드 | 신체 부위별 설정값 |
| `0x20` | 체압분산 | 레벨, 속도 |
| `0x31`~ | 마사지 (1~12종) | 신체 부위별 설정값 |
| `0x40` | 돌봄케어 - 머리감싸기 | - |
| `0x41` | 돌봄케어 - 배변 | - |
| `0x42` | 돌봄케어 - 이동(좌) | - |
| `0x43` | 돌봄케어 - 이동(우) | - |
| `0x50` | 온열 OFF | - |
| `0x51` | 온열 1단 | - |
| `0x52` | 온열 2단 | - |
| `0x53` | 온열 3단 | - |
| `0x60` | 통풍 | 단계 |
| `0x70` | 일시정지 | - |
| `0x71` | 재동작 | - |

#### CMD2: 설정값 전송 (CMD1=0x20)

설정값은 0x10 단위로 증가하며, 확인 키로 ESP32에 전송됨.

**시간 설정:**
| 값 | 시간 |
|----|------|
| 0x01 | 10분 |
| 0x02 | 20분 |
| 0x03 | 30분 |
| 0x04 | 1시간 |
| 0x05 | 6시간 |
| 0x06 | 12시간 |
| 0x07 | 24시간 |

**RPM 설정:**
| 값 | RPM |
|----|-----|
| 0x01 | 200 |
| 0x02 | 250 |
| 0x03 | 300 |

**높이 설정:** 30, 60, 90, 120, 150, 170 (직접 값)

**감도 설정:**
| 값 | 감도 |
|----|------|
| 0x01 | 4.2 kPa |
| 0x02 | 3.0 kPa |
| 0x03 | 2.5 kPa |
| 0x04 | 2.0 kPa |
| 0x05 | 1.5 kPa |
| 0x06 | 1.0 kPa |

### 8.5 통신 흐름

```
리모컨(adStar)                         ESP32(베드)
    │                                      │
    │─── [0x10] 전원 ON (0x80) ──────────►│
    │─── [0x10] 초기화 확인 (0xFF) ──────►│
    │─── [0x10] 스탠바이 (0xF0) ─────────►│
    │                                      │
    │  (사용자가 모드 선택 + 설정 후 확인)   │
    │                                      │
    │─── [0x20] 설정값 전송 ──────────────►│
    │─── [0x10] 동작 시작 ────────────────►│
    │                                      │
    │◄── [0x50] 바 상태 정보 (16개) ───────│  (주기적)
    │◄── [0x70] 신체 감지 정보 ────────────│  (주기적)
    │                                      │
    │─── [0x10] 전원 OFF (0x81) ─────────►│
    │                                      │
```

### 8.6 수신 타임아웃 처리

- 수신 시작 후 **1초** (100 x 10ms) 내 패킷 미완성 시 버퍼 초기화
- 링 버퍼 방식 (RX: 2048B, TX: 1024B)
- 동기 패턴(0xFF81 + "SB1C") 검색으로 패킷 시작점 탐색

---

## 9. 기능별 상세 설명

### 9.1 전원 제어

전원키는 **숏클릭**(릴리즈 판별)과 **롱클릭**(~3초 즉시 판별)으로 동작이 분리됩니다.

```
전원 상태 전이도:

    REMO_PWR_ON (0)         ← 초기 상태 (전원 미입력)
         │
         │ 전원 키 클릭 (power=true)
         │ → LCD_ON()
         │ → ESP32: PWR_ON (0x80)
         │ → ESP32: INIT (0xFF)
         │ → ESP32: STDBY (0xF0)
         │ → MODE_HOME
         ▼
    REMO_LCD_ON (1)         ← 정상 동작 상태
         │
         ├── 숏클릭 (릴리즈 시 LONG_KEY_CNT 미달)
         │   → ESP32: STDBY (0xF0)
         │   → MODE_HOME (홈 화면 표시)
         │   → stdby_in_progress = true (키 차단)
         │   → ESP32 ACK 수신 시 키 차단 해제
         │
         └── 롱클릭 (~3초, 즉시 실행)
             → REMO_STDBY_OFF (3)
             → ESP32: STDBY (0xF0)
             → MODE_SHUTDOWN ("종료합니다." 화면)
             → stdby_in_progress = true (모든 키 차단)
             → power = false
                  │
                  │ ESP32 ACK 수신 (또는 타임아웃 ~60초)
                  ▼
             REMO_LCD_OFF (2)
             → ESP32: PWR_OFF (0x81)
             → "종료합니다." 1.5초 표시
             → LCD_OFF()
                  │
                  │ 전원 키 클릭 (power=true)
                  │ → LCD_ON()
                  │ → ESP32: PWR_ON (0x80)
                  │ → MODE_HOME
                  ▼
             REMO_LCD_ON (1)         ← 복귀
```

#### 종료 화면 (MODE_SHUTDOWN)

롱클릭 시 홈 화면 대신 "종료합니다." 화면을 표시합니다. 검은 배경에 흰색 한글 텍스트로 렌더링되며, 초기위치 복귀가 완료될 때까지 유지됩니다. 종료 화면 표시 중에는 모든 키 입력이 차단됩니다 (`stdby_in_progress`).

```c
void shutdown_draw(void) {
    set_draw_target(getbackframe());
    draw_rectfill(0, 0, 320, 480, MAKE_COLORREF(0, 0, 0));  // 검은 배경
    egl_font_set_color(g_pFontKor, MAKE_COLORREF(255, 255, 255));
    draw_text_kr(g_pFontKor, 95, 226, "종료합니다.");
    flip();
}
```

### 9.2 교대부양 (MODE_LEVITATE)

3가지 서브 모드와 4개 신체 부위별 3가지 설정값:

```
서브 모드: 일반(NORMAL) / 집중(FOCUS) / 수면(SLEEP)
    ← → 키로 전환

신체 부위: 머리 / 상체(상) / 상체(하) / 다리
    ↑ ↓ 키로 선택

설정 항목 (각 부위별):
    - 시간 (TIME)
    - 높이 (HIGH)
    - 속도/RPM (RPM)
    ← → 키로 값 조절 (0x10 단위)
```

### 9.3 체압분산 (MODE_VAIRANCE)

2개 항목(레벨, 속도)에 대한 설정:

```
설정 항목:
    - 레벨 (LEVEL)
    - 속도 (SPEED)

바(에어셀) 16개의 상태를 ESP32로부터 수신하여 화면에 표시
```

### 9.4 마사지 (MODE_MASSAGE)

12종의 마사지 타입, 각 타입별 4개 부위 x 3가지 설정:

```
마사지 종류 (12종): ← → 키로 전환
    각 타입별 아이콘: top_nav_select.suf ~ top_nav_select12.suf

신체 부위별 설정: 교대부양과 동일 구조
```

### 9.5 돌봄케어 (MODE_PATIENT_CARE)

3가지 기능:

```
├── 머리감싸기 (HEAD_WRAP) → CMD2: 0x40
├── 배변 (DEFECATE)        → CMD2: 0x41
└── 이동 (SHIFT)           → CMD2: 0x42(좌), 0x43(우)
```

### 9.6 온열 (MODE_HEAT)

4단계 제어 (OFF/1단/2단/3단):
- 각 단계별 LED 1개씩 점등 (P3.0~P3.2)
- ESP32에 CMD2: 0x50(OFF) ~ 0x53(3단) 전송

### 9.7 통풍 (MODE_VENTILATION)

4단계 제어 (OFF/1단/2단/3단):
- 각 단계별 LED 1개씩 점등 (P3.3, P3.4, P4.4)
- ESP32에 CMD2: 0x60 + 단계값 전송

### 9.8 설정/저장 (MODE_SET_SAVE)

수동 설정 및 셀프 테스트:

```
├── 수동 (Manual)     → 수동 설정 모드
└── 셀프 테스트 (Self Test) → 하드웨어 자가 진단
    ├── ESP32 통신 테스트 (오디오, 온열, 통풍)
    ├── 키 입력 테스트
    └── LED 테스트
```

### 9.9 초기화 (MODE_INITIAL)

모든 모드의 설정값을 공장 초기값으로 복원:

```
확인 키 입력 시:
    ├── ESP32에 초기화 명령 전송
    ├── levitate_value_power_init()
    ├── dispersion_value_power_init()
    ├── massage_value_power_init()
    ├── patient_care_value_power_init()
    ├── heat_value_power_init()
    └── ventilation_value_power_init()
```

---

## 10. LED 제어

### 10.1 온열 LED (3개)

| LED | 포트 | 1단 | 2단 | 3단 |
|-----|------|-----|-----|-----|
| LED 1 | P3.2 | ON | OFF | OFF |
| LED 2 | P3.1 | OFF | ON | OFF |
| LED 3 | P3.0 | OFF | OFF | ON |

- Active Low (LOW=ON, HIGH=OFF)
- `heat_led_ctrl(0~3)` 함수로 제어

### 10.2 통풍 LED (3개)

| LED | 포트 | 1단 | 2단 | 3단 |
|-----|------|-----|-----|-----|
| LED 1 | P4.4 | ON | OFF | OFF |
| LED 2 | P3.4 | OFF | ON | OFF |
| LED 3 | P3.3 | OFF | OFF | ON |

- Active Low (LOW=ON, HIGH=OFF)
- `ventilation_led_ctrl(0~3)` 함수로 제어

---

## 11. 파일시스템 및 이미지 리소스

### 11.1 파일시스템

- **타입**: FAT (FatFS)
- **저장매체**: NAND Flash
- **마운트**: `f_mount(&fs, "0:", 1)`
- **앱 바이너리**: `boot.bin` (NAND FAT 루트)

### 11.2 이미지 파일 (.suf)

`.suf` (Surface) 파일은 adStar SDK 전용 이미지 포맷으로, RGB565로 프리디코딩된 raw 픽셀 데이터입니다.

**장점**: 디코딩 없이 바로 SURFACE 구조체에 로드 → 빠른 표시
**단점**: 비압축이라 파일 크기가 큼

#### 이미지 파일 목록

| 카테고리 | 파일명 | 용도 |
|----------|--------|------|
| **홈** | home.suf | 메인 홈 화면 |
| **공통** | bottom1.suf, bottom2.suf | 하단 네비게이션 바 |
| | btn_left.suf, btn_right.suf | 좌우 화살표 |
| | btn_left_run.suf, btn_right_run.suf | 동작 중 좌우 화살표 |
| | bar_active.suf | 에어셀 바 표시기 |
| | human.suf | 인체 다이어그램 |
| **라디오 버튼** | radio_available_selected_none.suf | 활성-미선택 |
| | radio_available_selected_focused.suf | 활성-선택됨 |
| | radio_disabled_selected_none.suf | 비활성-미선택 |
| | radio_disabled_selected_focused.suf | 비활성-선택됨 |
| **수치 표시** | disp_cnt_bg_available.suf | 활성 카운터 배경 |
| | disp_cnt_bg_focused.suf | 포커스 카운터 배경 |
| **체압분산** | rc_title_bar_vairance.suf | 타이틀 바 |
| | main_icon_vairance.suf | 메인 아이콘 |
| **교대부양** | rc_title_bar_levitate.suf | 타이틀 바 |
| | main_icon_levitate.suf | 메인 아이콘 |
| | top_normal_btn_default/focused.suf | 일반 모드 버튼 |
| | top_concent_btn_default/focused.suf | 집중 모드 버튼 |
| | top_sleep_btn_default/focused.suf | 수면 모드 버튼 |
| **마사지** | rc_title_bar_massage.suf | 타이틀 바 |
| | main_icon_massage.suf | 메인 아이콘 |
| | top_nav_select.suf ~ top_nav_select12.suf | 12종 마사지 타입 아이콘 |
| **돌봄케어** | rc_title_bar_care.suf | 타이틀 바 |
| | main_icon_care.suf | 메인 아이콘 |
| | top_head_wrap_btn_default/focused.suf | 머리감싸기 버튼 |
| | top_defecate_btn_default/focused.suf | 배변 버튼 |
| | top_shift_btn_default/focused.suf | 이동 버튼 |
| | head_wrap.suf | 머리감싸기 이미지 |
| | shift.suf | 이동 이미지 |
| **온열** | rc_title_bar_heat.suf | 타이틀 바 |
| | main_icon_heat.suf | 메인 아이콘 |
| | hit_on.suf, hit_off.suf | ON/OFF 표시 |
| **통풍** | rc_title_bar_ventilat.suf | 타이틀 바 |
| | main_icon_ventilat.suf | 메인 아이콘 |
| | wind_on.suf, wind_off.suf | ON/OFF 표시 |
| **설정** | rc_title_bar_set.suf | 타이틀 바 |
| | main_icon_set.suf | 메인 아이콘 |
| | top_manual_btn_default/focused.suf | 수동 모드 버튼 |
| | top_self_test_btn_default/focused.suf | 셀프테스트 버튼 |

#### 폰트 파일

| 파일명 | 크기 | 용도 |
|--------|------|------|
| font/font28.fnt | 28pt | 주요 텍스트 (설정값, 제목) |
| font/font16.fnt | 16pt | 보조 텍스트 (라벨) |

### 11.3 지원 이미지 포맷

| 포맷 | 로드 함수 | 라이브러리 | 용도 |
|------|-----------|-----------|------|
| .suf | `loadsurf()` | libadStar.a | UI 이미지 (주 사용) |
| .png | `loadpng()` | libpng.a + libz.a | 부트 이미지, 범용 |
| .jpg | `loadjpg()` | libjpeg.a | 범용 |
| .bmp | `loadbmp()` | libadStar.a | 범용 |

---

## 12. 빌드 환경

### 12.1 툴체인

| 도구 | 명령어 |
|------|--------|
| C 컴파일러 | ae32000-elf-gcc |
| 어셈블러 | ae32000-elf-as |
| 링커 | ae32000-elf-ld |
| 아카이버 | ae32000-elf-ar |
| 바이너리 변환 | ae32000-elf-objcopy |
| 디스어셈블러 | ae32000-elf-objdump |

### 12.2 컴파일 옵션

```
CFLAGS  = -O2 -g -fno-strength-reduce -Wall
ASFLAGS = -gstabs
LDFLAGS = -Wall -nostartfiles -lgcc -lc -lm
```

### 12.3 링크 라이브러리

| 라이브러리 | 용도 |
|-----------|------|
| libadStar.a | adStar SDK 코어 (그래픽, 타이머, GPIO 등) |
| libjpeg.a | JPEG 이미지 디코딩 |
| libpng.a | PNG 이미지 디코딩 |
| libz.a | zlib 압축 해제 (PNG용) |
| libmad.a | MP3 오디오 디코딩 (현재 미사용) |

### 12.4 빌드 출력

| 파일 | 설명 |
|------|------|
| `output/smart_bed_app.elf` | 앱 ELF 바이너리 |
| `output/smart_bed_app.elf.bin` | 앱 raw 바이너리 (→ NAND에 boot.bin으로 저장) |
| `output/bootloader.elf.bin` | 부트로더 바이너리 (→ NOR Flash에 저장) |

### 12.5 메모리 맵

```
NOR Flash (512KB)
0x00000000 ┌──────────────────┐
           │   Boot Loader    │
           │   (~80KB)        │
0x00014000 ├──────────────────┤  ← FLASH_APP_OFFSET (NOR 부팅 시)
           │   Application    │
           │   (나머지)        │
0x00080000 └──────────────────┘

RAM (7,936KB)
0x20000000 ┌──────────────────┐  ← 앱 로드 주소 (boot.bin 복사 대상)
           │   Application    │
           │                  │
0x20800000 ├──────────────────┤  ← 링커 스크립트 RAM 시작
           │   Stack / Heap   │
           │   Frame Buffers  │
           │   (320x480x2x2   │
           │    = ~600KB)     │
           └──────────────────┘

NAND Flash (FAT)
           ┌──────────────────┐
           │   boot.bin       │  ← 앱 바이너리
           │   image/         │  ← UI 이미지 (.suf)
           │   image/font/    │  ← 비트맵 폰트 (.fnt)
           └──────────────────┘
```

---

## 13. 알려진 이슈

### 13.1 SET_KEY 중복 처리 (key.c:192~204)

`key_read()` 함수에서 `SET_KEY`가 두 번 검사됨. 첫 번째 블록(192줄)에서 `return`하므로 두 번째 블록(199~204줄)은 **도달 불가능한 코드(dead code)**:

```c
// 첫 번째 체크 (192줄) - 항상 이것만 실행됨
if(!(remocon_key.current & SET_KEY)){
    remocon_key.key_val = SET_KEY;
    return;  // ← 여기서 항상 리턴
}
// 두 번째 체크 (199줄) - 도달 불가
if(!(remocon_key.current & SET_KEY)){
    if(smart_bed_status.status == MODE_HOME)
        smart_bed_status.status = MODE_SET_SAVE;
    remocon_key.key_val = SET_KEY;
    return;
}
```

**영향**: `home_proc()`에서 별도로 `SET_KEY` → `MODE_SET_SAVE` 전환 처리가 있어 기능적 문제는 없으나 코드 정리 필요.

### 13.2 display.h와 lcd.h 좌표 상수 중복

`include/display.h`와 `drive/lcd.h`에 동일한 UI 좌표 상수가 중복 정의되어 있으며, 일부 값이 다름:

| 상수 | display.h | lcd.h | 차이 |
|------|-----------|-------|------|
| `BAR_X` | 93 | 95 | 2px 차이 |
| `BAR_Y` | 212 | 214 | 2px 차이 |
| `LABLE0_X` | 242 | 232 | 10px 차이 |

`main.h`에서 `lcd.h`를 include하므로 실제 빌드에는 `lcd.h`의 값이 사용됨.

### 13.3 CRC16 체크 비활성화

`esp32_packet_parsing()` 함수에서 CRC16 검증 코드가 `#if 0`으로 비활성화되어 있음. 현재는 CRC 검증 없이 패킷을 수신하고 있어 데이터 무결성 검증이 이루어지지 않음.

---

*끝*
