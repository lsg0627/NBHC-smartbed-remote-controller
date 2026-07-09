# 리모컨 펌웨어 스펙 — 부팅 확인창 + 안전 홈 시퀀스

**현재 개정: v6**
대상 Master FW: v3.2.7 (`044d607`)
Master repo: https://github.com/lsg0627/NBHC-smartbed-main-board

이 문서의 리모컨 코드 인용은 전부 본 저장소(`smart_bed`) 기준으로 검증됨.
마스터 코드 인용(`bed_control.c` 등)은 별도 저장소이므로 **여기서 검증 불가** — 마스터 담당자 확인 필요.

---

## 0. 개정 이력

### v5 → v6 (현재)

`power` 전역이 부팅 시 `false`라는 사실이 v5 §5의 전제를 무너뜨렸다.

| # | 문제 | 처리 |
|---|---|---|
| 1 | **"부팅 → 확인창"이 불가능.** 부팅 시 `power=false`, LCD OFF ([`drive/key.c:20`](../drive/key.c#L20), [`app/main.c:50`](../app/main.c#L50)) | 확인창 표시 시점을 **전원키 첫 누름**으로 변경 (§5.1) |
| 2 | `REMO_PWR_ON` 경로가 `CMD2_PWR_ON`을 송신 → 마스터가 거부 → 5초 무반응 | `startup_pending==1`이면 **`CMD2_PWR_ON` 송신 억제** (§5.1) |
| 3 | 확인창 표시 중 전원키 숏클릭이 `CMD2_STDBY`를 보내버림 | 확인창 중 전원키 숏클릭 차단 (§5.2) |
| 4 | §10/T7의 "전원키 한 번으로 홈 시작" | 실제로는 **두 번**. 1번째는 거부됨 (§10) |
| 5 | GetBedStatus 명분이 "부팅 후 즉시 UI 복원" | 부팅 시 UI가 없다. 명분 축소 (§3.1) |

### v4 → v5

| # | 문제 | 처리 |
|---|---|---|
| 1 | 프레임 정의 전부 오류 (STX 1바이트 `0xF0`, Product Code 누락, `DataLen` 위치·값 오류, CRC 범위 오류) | §2 실코드 기준 재작성 |
| 2 | Path B 가드에 `&& stdby_in_progress` → T10을 스스로 배제 | 가드 제거 (§5.3) |
| 3 | GetBedStatus 송신 절차가 작업 목록에서 누락 | §3 복원 |
| 4 | 확인창 3초 long-press UI 사양 누락 | §6 복원 |
| 5 | 통신 끊김 감지(4.5초) 누락 | §7 복원 |
| 6 | `key.c:227`을 `stdby_timeout`으로 오기 (실제는 `bed_state_lock_remain`) | `key.c:225`로 정정 (§8) |
| 7 | T7 "데드락" 판정 오류 | "안전 우회"로 재작성 후 v6에서 재정정 (§10) |
| 8 | 세팅 순서 근거가 "인터럽트 경합" | 메인 루프는 협조적 폴링. 요구는 "둘 다 세팅"뿐 (§5.2) |
| 9 | tx hex 로그 누락 | §12 복원 |

---

## 1. 배경

전원 인가 직후 자동 홈은 매트리스 위에 환자가 있을 때 위험하다. 리모컨 확인창을 거쳐야 홈이 진행되도록 재설계한다.

Master v3.2.4~v3.2.7 흐름:

1. 마스터 부팅 → `read_position()`만 수행 (probe read-only, 홈 없음)
2. `startup_pending = 1` 상태로 대기
3. `BedStatus(0x40)` 브로드캐스트에 `startup_pending=1` 실림
4. 마스터는 `InitializeAction(CMD2_STDBY)` 외의 모든 명령을 거부
5. 리모컨이 확인창 표시 → 사용자 3초 확인
6. 리모컨이 `CMD2_STDBY(0xF0)` 송신
7. 마스터가 `startup_pending=0` 클리어 후 홈 시퀀스 시작 (`run_state=3`)
8. **홈 완료 시** 마스터가 `ActionEcho(0x10, 0xF0)` 1회 발송

---

## 2. RS232 프레임 (실코드 기준)

```
┌─────────┬──────────────┬──────┬──────┬─────────┬──────────┬──────────┐
│   STX   │ Product Code │ CMD1 │ CMD2 │ DataLen │   Data   │ Checksum │
│   2B    │      4B      │  1B  │  1B  │   1B    │  0~246B  │    2B    │
└─────────┴──────────────┴──────┴──────┴─────────┴──────────┴──────────┘
  offset 0       2          6      7        8         9        9+DataLen
```

- **STX**: `0xFF 0x81` (2바이트). [`drive/protocol.h:18`](../drive/protocol.h#L18), [`drive/protocol.c:265`](../drive/protocol.c#L265)
- **Product Code** (4바이트, 방향에 따라 다름):
  - 리모컨 → 마스터: `"SB1C"` = `53 42 31 43` — [`drive/protocol.c:265-270`](../drive/protocol.c#L265-L270)
  - 마스터 → 리모컨: `"9B1C"` = `39 42 31 43` — [`drive/protocol.c:293`](../drive/protocol.c#L293), [`drive/protocol.c:518`](../drive/protocol.c#L518)
- **DataLen**: Data 바이트 수 그 자체 (`N`). 헤더 길이를 포함하지 않는다. [`drive/protocol.c:274`](../drive/protocol.c#L274)
- **Checksum**: 2바이트, 위치는 `offset 9 + DataLen`. [`drive/protocol.c:280`](../drive/protocol.c#L280)

전체 패킷 길이 = `DataLen + 11`. [`drive/protocol.c:286`](../drive/protocol.c#L286)
UART: 115200, 8N1.

### 2.1 CMD1 / CMD2

| 상수 | 값 | 정의 위치 |
|---|---|---|
| `CMD1_SEND_RUN_ST` | `0x10` | [`drive/protocol.h:22`](../drive/protocol.h#L22) |
| `CMD1_BED_STATUS` | `0x40` | [`drive/protocol.h:25`](../drive/protocol.h#L25) |
| `CMD2_PWR_ON` | `0x80` | [`drive/protocol.h:39`](../drive/protocol.h#L39) |
| `CMD2_STDBY` (초기화 실시) | `0xF0` | [`drive/protocol.h:42`](../drive/protocol.h#L42) |
| `CMD2_INIT` (초기화 confirm) | `0xFF` | [`drive/protocol.h:41`](../drive/protocol.h#L41) |
| `GetBedStatus` (신규) | `0xF1` | — |

> **CMD1은 반드시 `0x10`이다. `0x01`이 아니다.**
> `0x01`은 존재하지 않는 CMD1이다. 잘못 보내면 마스터가 조용히 무시하고, 리모컨은 재시도 실패 후 fallback으로 정상 진입한다. 즉 **겉보기엔 동작하지만 GetBedStatus가 한 번도 성공하지 않는다.** 이 증상은 구버전 마스터 접속(T6)과 구분되지 않는다. §12의 tx hex 로그로 반드시 확인할 것.

> `0xF0`(`CMD2_STDBY`, "초기화 실시")과 `0xFF`(`CMD2_INIT`, "초기화 confirm")는 다른 값이다. 혼동 금지.

### 2.2 CRC-16 (표준 아님 — 소스 복사 필수)

구현: [`drive/protocol.c:212-237`](../drive/protocol.c#L212-L237)

| 항목 | 값 |
|---|---|
| init | `0xFFFF` |
| polynomial | `0xA001` |
| final XOR | 없음 |
| bit order | LSB-first (reflected) |
| 바이트당 시프트 | **9회** — `for(j=0; j<=8; j++)` [`drive/protocol.c:222`](../drive/protocol.c#L222) |
| 계산 범위 | `tx_buff[0]` ~ Data 끝. **STX와 Product Code를 포함**하고 Checksum만 제외. 길이 = `DataLen + 9` [`drive/protocol.c:279`](../drive/protocol.c#L279) |
| 저장 | 함수 반환값을 `memcpy`로 그대로 기록. 추가 변환 금지 [`drive/protocol.c:280`](../drive/protocol.c#L280) |

> ⚠️ **표준 CRC-16/Modbus 라이브러리를 쓰지 말 것.** 표준은 바이트당 8회 시프트다. 여기는 9회다. 함수 위 주석의 `CRC CCITT : 0x1021`도 실제 코드와 맞지 않는다. `agms_calc_crc16` 소스를 그대로 복사하고 `j<=8`을 "고치지" 말 것.

> 함수는 내부에서 바이트 스왑 후 반환한다([`drive/protocol.c:231-233`](../drive/protocol.c#L231-L233)). 반환값을 `memcpy`로 저장하면 와이어에 big-endian으로 나간다. 스왑을 다시 하지 말 것.

**Data 상한 246바이트**: `agms_calc_crc16`의 `length`가 `U8`이다. `DataLen + 9 > 255`면 wrap 한다. 현 스펙의 패킷은 `DataLen=0`이라 무관하지만 payload 확장 시 제약이 된다.

### 2.3 RX CRC 검증은 이 작업 범위 밖

수신 측 CRC 검증은 [`drive/protocol.c:317-325`](../drive/protocol.c#L317-L325)에서 `#if 0`으로 비활성화되어 있다. 활성화는 별도 이슈(§13). 본 스펙의 응답 판정에 checksum 검증을 포함하지 않는다.

---

## 3. 작업 1 — 부팅 후 GetBedStatus 요청

**시점**: 리모컨 자체 부팅 완료 후 100~300ms 이내. **LCD는 꺼진 상태다** — 화면 출력 없이 송신만 한다.

**송신 패킷 (11바이트)**:

```
FF 81 53 42 31 43 10 F1 00 [CRC_hi] [CRC_lo]
                  ^^ ^^
                  |  └─ CMD2 = 0xF1 (GetBedStatus, 신규)
                  └──── CMD1 = 0x10 (CMD1_SEND_RUN_ST)  ← 0x01 아님
```

**응답 대기**: 200ms × 최대 3회 재시도. (틱 100ms이므로 2틱 — §8)
**3회 실패 시 fallback**: 브로드캐스트(2초 주기) 대기. 이후 §5 분기는 브로드캐스트로 진입한다.

**응답 판정 조건** (checksum 검증 제외):

1. STX가 `FF 81`
2. Product Code가 `39 42 31 43`
3. CMD1 = `0x40`
4. DataLen ≥ 10

> fallback 로직은 필수다. GetBedStatus를 인식하지 못하는 마스터(v3.2.4 이하)와 연결해도 정상 동작해야 한다.

### 3.1 GetBedStatus가 실제로 주는 이득 (명분 정정)

리모컨은 `power == false`(LCD OFF) 상태에서도 브로드캐스트를 계속 수신해 `bed_status`를 갱신한다.
**검증**: `CMD1_BED_STATUS` 파싱 블록([`drive/protocol.c:693`](../drive/protocol.c#L693) 이하)에 `power` 게이트가 없고, `process_analy_data()`는 메인 루프에서 무조건 호출된다([`app/main.c:70`](../app/main.c#L70)). 코드 주석도 같은 내용을 명시한다([`code/user_task.c:757`](../code/user_task.c#L757)).

따라서 사용자가 전원키를 누를 즈음이면 `bed_status`는 대개 이미 최신이다.

**GetBedStatus가 이득인 구간은 하나뿐이다**: 리모컨 부팅 후 브로드캐스트 1주기(2초) 이내에 사용자가 전원키를 누르는 경우 — 즉 T1(전체 전원 인가 직후, 사용자가 곧바로 전원키를 누름).

비용이 싸므로 유지한다. 다만 v5까지 적혀 있던 **"부팅 후 즉시 UI 복원"은 틀린 명분이다.** 부팅 시점에는 UI 자체가 없다.

---

## 4. 작업 2 — BedStatus payload 해석

`CMD1_BED_STATUS(0x40)`, 10바이트. 구조체 [`drive/protocol.h:105-115`](../drive/protocol.h#L105-L115). 브로드캐스트 주기 2초 ([`drive/protocol.h:104`](../drive/protocol.h#L104)).

| offset | 필드 | 값 |
|---|---|---|
| [0] | `current_mode` | `0x00`=정지, `0x10`=교대일반, `0x36`=트위스트([`drive/smart_bed_remocon.c:134`](../drive/smart_bed_remocon.c#L134)) … |
| [1] | `run_state` | 0=정지, 1=동작중, 2=일시정지, 3=초기화중. **4는 사용하지 않는다** |
| [2] | `backrest_percent` | 0~100 |
| [3] | `leg_panel_percent` | 0~100 |
| [4] | `heat_level` | 0=OFF, 1=약, 2=중, 3=강 |
| [5] | `volume_level` | 0=Mute, 1=Low, 2=Mid, 3=Max |
| [6] | `fall_state` | **현재 리모컨 미사용** (§9) |
| [7] | `powered_on` | 0=OFF, 1=ON |
| [8] | **`startup_pending`** | 0=정상, 1=사용자 확인 필요 — **확인창의 단일 진실 원천** |
| [9] | `reserved` | 0 |

**단일 진실 원천**: 확인창 표시 여부는 오직 `startup_pending`으로 판정한다. `run_state=4` 분기를 넣지 말 것 (v3.2.6에서 제거됨).

**하위호환**: 수신부가 `dlen >= sizeof(BED_STATUS_DATA)`로 게이트한다([`drive/protocol.c:696`](../drive/protocol.c#L696)). payload가 늘어나도 구버전 리모컨은 깨지지 않는다. 역으로 **DataLen이 10 미만이면 패킷 전체가 드롭된다** — §10.1 벤치 확인 항목.

**상태 잠금과의 상호작용**: `bed_state_lock_remain`이 살아 있는 동안 [`drive/protocol.c:705-712`](../drive/protocol.c#L705-L712)는 `current_mode`와 `run_state`만 이전 값으로 덮어쓴다. `startup_pending`은 그대로 통과한다. 따라서 T4/T9(pending 감지)는 잠금 구간에서도 동작한다.

### 4.1 ActionEcho 타이밍 계약 (마스터 측 보장)

- 마스터는 `CMD2_STDBY` **수신 시점에 에코하지 않는다.**
- `ActionEcho(0x10, 0xF0)`는 홈 완료(`BarsInitialized`) 시 **1회만** 발송한다.
- **단, `action == BedActionCare`이면 발송하지 않는다.**

**Q2 확정 (마스터 전수 조사)**: `CMD1=0x10 AND CMD2=0xF0` 조합의 유일한 발송 지점은 `bed_control.c:1606-1607`이며, `BarsInitialized` 케이스 내부의 `if (action != BedActionCare)` 게이트 안에 있다. `ActionEcho` 발송 8곳, `InitializeAction(0xF0)` 3곳을 교차 확인한 결과다.

> 이 계약이 깨지면 부팅 흐름이 즉시 무너진다. [`code/user_task.c:810`](../code/user_task.c#L810)은 ACK 수신을 **홈 완료**로 해석하고 `run_state`를 0으로 강제한다. 마스터가 수신 즉시 에코하면 리모컨은 홈이 시작되자마자 침대를 "대기중"으로 표시한다.

> ⚠️ **케어 모드 예외가 Path A를 무력화한다.** 돌봄케어 모드 중 홈이 완료되면 ActionEcho가 오지 않는다. Path A만 구현하면 `stdby_in_progress`가 타임아웃(110초)까지 유지되어 "초기화중" 화면이 멈춘 것처럼 보인다. §8에서 `key.c:225`를 300→1100으로 올리므로 이 증상은 **30초에서 110초로 악화된다.** **Path B(§5.3)가 이 경우의 유일한 완료 신호다.**

---

## 5. 작업 3 — UI 상태 머신

### 5.1 부팅 및 전원키 흐름

```
[리모컨 부팅]  power=false, LCD OFF
   │
   ├─ GetBedStatus(0x10, 0xF1) 송신 — 200ms × 3 재시도
   ├─ 응답 or fallback → bed_status 캐시 갱신 (화면 출력 없음)
   └─ 이후 브로드캐스트(2초)로 계속 갱신
   │
   ▼
[사용자가 전원키 첫 누름]   power=true, LCD ON
   │
   ├── startup_pending == 1 ──┐
   │      CMD2_PWR_ON 송신하지 않는다  ← [중요]
   │      확인창 표시 (3초 long-press, §6)
   │         │
   │         ▼ 사용자 확인
   │      §5.2 송신 시퀀스 → 홈 진행 화면
   │         │
   │         ▼
   │      §5.3 완료 감지 (Path A 또는 Path B) → 일반 UI
   │
   └── startup_pending == 0 ──┐
          기존 REMO_PWR_ON 경로 그대로
          (bed idle이면 CMD2_PWR_ON 송신)
          → current_mode / run_state 기반 UI 복원
```

**왜 부팅 시점이 아닌가**: 부팅 시 `power = false`이고([`drive/key.c:20`](../drive/key.c#L20)) `LCD_ON()`은 주석 처리되어 있다([`app/main.c:50`](../app/main.c#L50)). 화면이 꺼져 있어 확인창을 띄울 대상이 없다. 또한 `CONFORM_KEY`는 `power == false`면 처리 전에 차단된다([`drive/key.c:397`](../drive/key.c#L397)) — 확인 입력 자체가 불가능하다.

**왜 `CMD2_PWR_ON`을 억제해야 하는가**: `startup_pending=1`인 마스터는 `InitializeAction` 외 모든 명령을 거부한다. 현재 [`code/user_task.c:755-766`](../code/user_task.c#L755-L766)의 `REMO_PWR_ON`은 침대가 idle이면 `CMD2_PWR_ON`을 보내고([`code/user_task.c:762`](../code/user_task.c#L762)) `awaiting_homing_start`로 5초를 기다린다([`code/user_task.c:764`](../code/user_task.c#L764)). 마스터가 거부하므로 **5초 스피너 후 아무 일도 일어나지 않는다.** 사용자에겐 고장으로 보인다.

**LCD가 꺼진 동안 `pending 0→1` 전이가 오면**(마스터 재부팅·watchdog) 화면 표시는 하지 않는다. 플래그만 갱신하고, 다음 전원키 누름에서 확인창으로 진입한다.

### 5.2 확인 후 송신 시퀀스

```c
// 세 필드를 메인 루프로 복귀하기 전에 모두 세팅해야 한다.
stdby_timeout     = STDBY_TIMEOUT_TICKS;   // 1100 (110초) — §8
stdby_in_progress = true;
esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_STDBY, &tmp, 0);   // FF 81 53 42 31 43 10 F0 00 CRC
```

**요구사항은 "둘 다 세팅"이다.** `stdby_timeout`의 전역 초기값은 `0`이고([`code/user_task.c:746`](../code/user_task.c#L746)), `check_stdby_progress()`는 `stdby_timeout == 0`이면 즉시 타임아웃 분기로 들어간다([`code/user_task.c:831`](../code/user_task.c#L831)). `stdby_in_progress`만 켜면 다음 틱에 강제 완료 처리되어 홈 진행 중 UI가 무너진다.

세 줄의 상대 순서는 무관하다. 메인 루프는 협조적 폴링이고([`app/main.c:62-70`](../app/main.c#L62-L70)) `check_stdby_progress()`는 ISR이 아니다. 기존 [`drive/key.c:223-228`](../drive/key.c#L223-L228)도 `in_progress → timeout → send` 순서로 정상 동작한다. **인터럽트 경합을 근거로 순서를 강제하지 말 것.**

### 5.2.1 확인창 표시 중 키 차단 (신규 — v6)

확인창이 떠 있는 동안:

| 키 | 처리 |
|---|---|
| `CONFORM_KEY` 3초 long-press | 확인 → §5.2 실행 |
| **전원키 숏클릭** | **차단.** 막지 않으면 [`drive/key.c:220-232`](../drive/key.c#L220-L232)가 확인 없이 `CMD2_STDBY`를 보낸다 |
| 전원키 롱클릭 | 허용 = 확인 취소 + 전원 OFF ([`drive/key.c:186-198`](../drive/key.c#L186-L198)) |
| 그 외 모든 키 | 무시 |

`MODE_FALL_ALERT` 게이팅([`code/user_task.c:868-878`](../code/user_task.c#L868-L878))과 동일한 방식으로 구현한다.

### 5.3 홈 완료 감지 — 두 경로

**Path A — ActionEcho 수신** (리모컨이 `CMD2_STDBY`를 보낸 경우)

기존 로직 재사용. [`drive/protocol.c:598`](../drive/protocol.c#L598)의 에코 핸들러가 `stdby_complete = true`를 세우고, [`code/user_task.c:810`](../code/user_task.c#L810)의 `check_stdby_progress()`가 완료 처리한다. **새 화면을 만들지 말 것.**

> **Path A가 오지 않는 경우가 두 가지다.** (1) 리모컨이 `CMD2_STDBY`를 보내지 않았을 때(T10) — 에코가 orphan으로 버려진다. (2) **돌봄케어 모드 중 홈일 때(§4.1)** — 마스터가 에코 자체를 보내지 않는다. 두 경우 모두 Path B가 유일한 완료 신호다. **Path B는 선택이 아니라 필수다.**

**Path B — 브로드캐스트 관찰** (Path A가 오지 않는 모든 경우)

```c
if (prev_run_state == 3 && run_state == 0 && startup_pending == 0) {
    // 홈 완료. stdby_in_progress 값과 무관하게 성립한다.
    stdby_in_progress = false;
    // 일반 UI 전환
}
```

> **가드에 `&& stdby_in_progress`를 넣지 말 것.** T10에서는 리모컨이 `CMD2_STDBY`를 보낸 적이 없어 `stdby_in_progress == false`이고, 홈 완료 후 도착하는 ActionEcho는 [`drive/protocol.c:598-606`](../drive/protocol.c#L598-L606)의 `orphan, ignored` 분기로 버려진다. Path B가 유일한 완료 신호다.

> **`startup_pending == 0` 가드는 필수다.** 마스터 watchdog 복구는 `is_homing=false`와 `startup_pending=true`를 같은 블록에서 세팅하므로, 한 브로드캐스트에 `run_state 3→0`과 `pending 0→1`이 **동시에** 실린다. 이 가드가 없으면 watchdog 복구를 홈 완료로 오인해 확인창 대신 일반 UI로 들어간다. (근거: `044d607:bed_control.c:1396-1404`)

**우선순위**: 한 패킷에서 §5.4(pending `0→1`)가 Path B보다 먼저 평가된다.

Path A와 Path B는 동시에 살아 있을 수 있다. **먼저 발화한 쪽이 완료 처리하고 나머지는 무시되도록 멱등하게 구현할 것** (`stdby_in_progress`가 이미 false면 no-op).

**홈 진행 중 화면**: [`code/user_task.c:204`](../code/user_task.c#L204)에 `homing_in_progress = (power && current_mode == 0 && run_state == 3)` 조건의 스피너가 이미 있다. 전원키를 눌러 `power=true`가 된 뒤라면 T10에서도 별도 구현 없이 화면이 나온다.

> `run_state=3`은 홈 전용이 아니다. 마스터의 유도식은 `if (action == BedActionFinalize || is_homing) → 3`이다. Finalize 중에도 3이 된다. Path B의 `3→0`이 Finalize 완료에도 발화하지만 결과(일반 UI 전환)가 같으므로 무해하다. 다만 **Finalize 중 `current_mode`가 0이면** 스피너가 "홈 진행 중"으로 잘못 뜬다 — §13 확인 항목.

### 5.4 pending 재발생 감지 (마스터 watchdog 복구 / 마스터 재부팅)

```c
if (prev_startup_pending == 0 && startup_pending == 1) {
    stdby_in_progress = false;
    stdby_timeout     = 0;
    if (power) { /* 확인창 재표시 */ }
    // power == false 이면 플래그만 갱신. 다음 전원키 누름에서 확인창.
}
```

**Q1 확정 (마스터 v3.2.7)**: watchdog 복구 시 `run_state`는 자동으로 0이 된다. 복구 블록이 `action = BedActionStop`, `is_homing = false`를 세팅하고, 유도식이 `if (action == BedActionFinalize || is_homing) → 3` 이므로 else 분기로 떨어진다. **확인창과 홈 스피너가 겹칠 우려는 없다.** (근거: `044d607:bed_control.c:1396-1404`, `:1324-1332`)

타임아웃 1100(110초)이 검증되는 지점이기도 하다. watchdog이 90초에 발화해 이 분기가 `stdby_in_progress`를 리셋하므로, 리모컨 자체 타임아웃 110초는 발화하지 않는다. 900(90초)이었으면 경합했다.

---

## 6. 작업 4 — 확인창 UI

```
┌─────────────────────────────────┐
│                                 │
│    ⚠️  침대를 시작합니다        │
│                                 │
│  환자가 침대에서 편안한 위치에  │
│  있는지 확인해 주세요.          │
│                                 │
│  ┌──────────────────────────┐   │
│  │  [확인]을 3초간 누르세요 │   │
│  │      ●○○○○○○○○○         │   │
│  └──────────────────────────┘   │
│                                 │
└─────────────────────────────────┘
```

**진입 조건** (둘 중 하나):
- 전원키 첫 누름으로 `power`가 `false → true`가 될 때 `startup_pending == 1`
- `power == true`인 상태에서 `startup_pending`이 `0 → 1`로 전이 (§5.4, T4/T9)

**확인 방법**: `CONFORM_KEY` 3초 long-press ([`drive/key.c:399`](../drive/key.c#L399)).
- 단일 클릭 금지 — 환자/간병인의 우발적 접촉으로 침대가 움직이면 안 된다.
- progress bar 10칸, **300ms마다 1칸** (틱 100ms 기준 3틱).
- 3초를 채우면 §5.2 시퀀스 실행.
- 3초 전에 손을 떼면 progress reset, 확인창 유지.

**이탈 조건**:
- 3초 확인 완료 → 홈 진행 화면 (§5.3의 기존 스피너 재사용)
- 전원키 롱클릭 → 확인 취소 + 전원 OFF
- 낙상 알람 발생 → `MODE_FALL_ALERT` 화면 (§9, T8)

---

## 7. 작업 5 — 통신 끊김 감지

브로드캐스트 주기는 2초다 ([`drive/protocol.h:104`](../drive/protocol.h#L104)).

- 마지막 `BedStatus` 수신 후 **4.5초**(45틱) 무수신 → "연결 확인 중…" 표시
- 재수신되면 다음 브로드캐스트 내용으로 자동 복원
- **GetBedStatus를 재송신하지 않는다.** 마스터가 살아 있으면 브로드캐스트가 다시 온다.

> 3초는 너무 촘촘하다. 1주기 지터만으로도 오탐한다.
> `power == false`(LCD OFF)일 때는 표시하지 않는다. 플래그만 유지한다.

---

## 8. 타임아웃 값

**틱 = 100ms.** 근거: [`code/user_task.c:192`](../code/user_task.c#L192)의 `time_10msec_interval_get(...) > 10` 게이트. 같은 핸들러의 `loading_remain=30`("~3초"), `awaiting_homing_timeout=50`("5초"), `external_stopping_timeout=200`("20초")과 일치한다.

| 위치 | 현재 값 | 권장 | 근거 |
|---|---|---|---|
| 신규 상수 `STDBY_TIMEOUT_TICKS` | — | **1100** (110초) | 마스터 watchdog 90초 + 브로드캐스트 2초 + 마진 20초. 900이면 마스터 복구 브로드캐스트 도착 전에 리모컨 타임아웃이 발화해 경합한다 |
| [`drive/key.c:225`](../drive/key.c#L225) `stdby_timeout = 300` | 300 (30초) | **1100** | 확인창 흐름이 이 경로를 재사용한다. 마스터가 홈을 90초까지 견디므로 30초 타임아웃은 **정상 홈 도중에 발화**한다. `stdby_in_progress=false`, `run_state` 강제 0 → 침대는 움직이는데 리모컨은 "대기중". T1 실패 |
| [`code/user_task.c:800`](../code/user_task.c#L800) `stdby_timeout = 6000` | 6000 (**600초**) | 본 스펙 범위 밖 | `REMO_STDBY_OFF`(전원키 롱클릭 종료) 경로. 주석의 "60초"는 10ms 틱을 가정한 것으로 틀렸다. 타임아웃 시 실제로 전원을 끄므로 **300으로 낮추면 홈 도중 전원이 나간다.** 별도 이슈로 1100 권장 |

> [`drive/key.c:227`](../drive/key.c#L227)은 `bed_state_lock_remain = 100`이다. `stdby_timeout`이 아니다. 이 줄을 건드리지 말 것.

---

## 9. 낙상 (`fall_state` offset [6])

**현재 낙상 UI는 비활성이다.** 낙상 감지 기능이 아직 동작하지 않아 경고 화면을 띄우지 않는다. [`drive/protocol.h`](../drive/protocol.h)의 `FALL_ALERT_UI_ENABLED 0`으로 제어하며, 기능이 살아나면 `1`로 바꾸면 된다.

- `CMD2_FALL_ALERT(0xA0)` 수신 → 화면 전환 없이 로그만 남긴다.
- `CMD2_FALL_CLEAR(0xA1)` 수신 → `MODE_FALL_ALERT` 상태일 때만 반응한다. 그렇지 않으면 무시한다. (게이트가 없으면 낙상 화면이 뜨지도 않은 상태에서 해제 패킷 하나로 아무 모드에서나 홈으로 튕긴다.)

관련 로직은 제거하지 않고 남겨 두었다. 재활성화 시 아래가 그대로 성립한다.

- `bed_status.fall_state`(offset [6])는 리모컨 코드에서 참조하지 않는다. 낙상은 `smart_bed_status.status == MODE_FALL_ALERT`로만 관리된다([`code/user_task.c:958`](../code/user_task.c#L958)). offset [6]을 새 진입점으로 삼지 말 것.
- 낙상 알람은 **확인창보다 우선**한다. 확인창 표시 중 낙상이 발생하면 `startup_confirm_active` 플래그만 유지하고 화면은 낙상 경고로 넘긴다. 해제되면 확인창으로 복귀한다.
- 확인창의 키 차단 블록은 `MODE_FALL_ALERT`를 반드시 제외해야 한다([`drive/key.c:254`](../drive/key.c#L254)). 제외하지 않으면 낙상 해제 키(`Pause`)까지 막혀 알람을 끌 수 없다.

---

## 10. 하위호환

| Master FW | GetBedStatus(0xF1) | `startup_pending` | 홈 완료 ACK | 90초 watchdog |
|---|---|---|---|---|
| v3.2.7 (권장) | ✅ 즉시 응답 | ✅ emit | ✅ | ✅ |
| v3.2.6 | ✅ | ✅ | ✅ | ❌ |
| v3.2.5 | ✅ | ✅ | ✅ | ❌ |
| v3.2.4 | ❌ 응답 없음 | ✅ | ✅ | ❌ |
| v3.2.3 이하 | ❌ | ❌ (0 또는 필드 없음) | ✅ | ❌ |

리모컨은 fallback 필수. `startup_pending` 부재 시 0으로 취급한다.

### 10.1 롤아웃 순서: **리모컨을 먼저 업그레이드한다**

신 마스터(v3.2.7) + 구 리모컨 조합에서 실제로 일어나는 일:

| 단계 | 동작 | 결과 |
|---|---|---|
| 1 | 전원키 **1번째** 누름 | `power == false` 분기([`drive/key.c:389-392`](../drive/key.c#L389-L392)) → `CMD2_PWR_ON(0x80)` 송신, `power = true` |
| 2 | 마스터 | `InitializeAction` 게이트가 PWR_ON **거부** |
| 3 | 리모컨 | `awaiting_homing_start` 5초 스피너([`code/user_task.c:764`](../code/user_task.c#L764)) → 타임아웃 → **무반응** |
| 4 | 전원키 **2번째** 짧게 누름 | `power == true` → [`drive/key.c:228`](../drive/key.c#L228) → `CMD2_STDBY` 송신 |
| 5 | 마스터 | 게이트 통과 → **확인창 없이 홈 시작** |

즉 **한 번이 아니라 두 번**이며, 문제가 두 가지다.

- **가용성**: 전원을 켜도 5초간 반응이 없다. 사용자는 고장으로 인식한다.
- **안전**: 두 번째 누름에서 확인창 없이 홈이 시작된다. 이 스펙이 도입하려는 안전 절차가 우회된다.

구버전 마스터에서는 `CMD2_PWR_ON`이 곧 홈 복귀 루틴을 트리거했다([`code/user_task.c:756`](../code/user_task.c#L756) 주석). 신 마스터가 그것을 막으면서 전원 ON의 정상 동작이 깨진 것이다. **필드에 이 조합이 나와서는 안 된다.**

### 10.2 벤치 확인 필요 (환자 없이)

1. **구버전 마스터(v3.2.3)가 `CMD2=0xF1`을 안전하게 무시하는가.**
   `CMD1=0x10`은 모든 버전이 인식하는 유효 명령이므로 이 패킷은 구버전 마스터의 CMD2 분기까지 도달한다. unknown CMD2의 default 처리가 no-op이라는 보장이 없다. 마스터 로그의 `Unknown Action: 0xF1` 확인 + 모터 무동작 실측.
   *(참고: 리모컨 수신부 switch에는 `default:` 절이 하나도 없다. 마스터가 같은 스타일이면 무시되겠지만 확인 전엔 알 수 없다.)*

2. **구버전 마스터가 `DataLen ≥ 10`으로 BedStatus를 보내는가.**
   미달이면 [`drive/protocol.c:696`](../drive/protocol.c#L696)의 게이트가 패킷 전체를 드롭해 fallback조차 성립하지 않는다.

---

## 11. 테스트 케이스

| # | 시나리오 | 예상 동작 / 판정 |
|---|---|---|
| T1 | 전체 전원 인가 (환자 있음) | 부팅(LCD OFF) → GetBedStatus → `pending=1` 캐시 → **전원키 누름** → 확인창 → 3초 확인 → `CMD2_STDBY` → 홈 → ActionEcho → 일반 UI. **`CMD2_PWR_ON`이 송신되지 않을 것**(tx 로그), 홈 도중 "대기중" 오표시 없을 것 |
| T2 | 리모컨만 재부팅 (마스터 마사지 실행 중) | LCD OFF 상태로 브로드캐스트 수신 → 전원키 → `pending=0, run_state=1, mode=0x36` → bed idle 아니므로 `CMD2_PWR_ON` 억제 → 마사지 UI 복원. 진행 시간은 payload에 없으므로 복원하지 않음 |
| T3 | 리모컨만 재부팅 (마스터 대기 중), Master v3.2.7 | 전원키 → `pending=0, run_state=0` → 기존 `REMO_PWR_ON` 경로. **판정(양성): GetBedStatus 응답이 실제로 수신될 것.** 미수신이면 CMD1 오타(`0x01`) 의심 |
| T4 | 마스터 OTA 후 재부팅 (리모컨 켜져 있음) | `pending 0→1` 전이 감지 → 확인창 즉시 표시 |
| T5 | RS232 케이블 순간 탈착 | 4.5초 후 "연결 확인 중" → 재접속 시 자동 복원. GetBedStatus 재송신 없음 |
| T6 | 구버전 마스터(v3.2.3) 접속 | GetBedStatus 무응답 → 3회 재시도 → fallback → 브로드캐스트로 진입. **판정(음성): crash·무한대기 없음.** tx hex 로그로 CMD1=`0x10` 확인 |
| T7 | 신 마스터 + 구 리모컨 | 1번째 전원키 → 5초 무반응. 2번째 숏클릭 → 확인창 없이 홈. **가용성 + 안전 둘 다 실패.** 롤아웃 순서 검증용 (§10.1) |
| T8 | 확인창 표시 중 낙상 알람 | **현재 스킵** — 낙상 UI 비활성(`FALL_ALERT_UI_ENABLED 0`, §9). 재활성화 시: `MODE_FALL_ALERT`로 즉시 전환, 해제 후 확인창 복귀 |
| T9 | 홈 진행 중 driver stall → 90초 초과 | 마스터 watchdog → `run_state 3→0` + `pending 0→1` 동시 브로드캐스트 → §5.4가 Path B보다 먼저 평가되어 확인창 재표시. 마스터 발화 후 최대 2초 이내 |
| T10 | 홈 진행 중 리모컨만 재부팅 | LCD OFF → 전원키 → `pending=0, run_state=3` → 홈 스피너. 홈 완료 시 **Path B**(`run_state 3→0`)로 완료 감지 → 일반 UI. *Path A는 orphan으로 버려지므로 Path B 없으면 실패* |
| T11 | 3초 확인 도중 손 떼기 | progress reset, 확인창 유지 |
| T12 | **확인창 표시 중 전원키 숏클릭** | **무시.** `CMD2_STDBY`가 나가지 않을 것 (tx 로그 확인). 롱클릭은 전원 OFF로 동작 |
| T13 | **돌봄케어 모드 중 전원키 숏클릭 → 홈** | 마스터가 ActionEcho를 보내지 않음(§4.1 케어 예외) → **Path B로 완료 감지.** "초기화중" 화면이 110초간 멈추면 Path B 미구현 |

---

## 12. 로그 (필수)

`0x01` 오타는 fallback 때문에 조용히 묻힌다. **송신 바이트를 hex로 남길 것.**

```
[BOOT] TX: FF 81 53 42 31 43 10 F1 00 [CRC] — GetBedStatus request
[RX]   FF 81 39 42 31 43 40 ... dlen=10
[RX]   pending=1, run_state=0, mode=0x00      (LCD OFF, 화면 없음)
[KEY]  POWER FIRST PRESS -> power=true, LCD ON
[UI]   pending=1 -> suppress CMD2_PWR_ON, show confirmation dialog
[USER] confirmed (3s hold complete)
[TX]   FF 81 53 42 31 43 10 F0 00 [CRC] — CMD2_STDBY
[RX]   pending=0, run_state=3 — homing
[UI]   homing screen (reusing stdby_in_progress path)
[RX]   ACK: STDBY COMPLETE   (Path A)
[UI]   normal UI
```

7번째 바이트가 `10`인지 매번 확인한다.

---

## 13. 미해결 / 후속

**벤치 확인 필요 (환자 없이 선행)**

- **B1**: 구버전 마스터가 `CMD2=0xF1`을 안전하게 무시하는가 (§10.2)
- **B2**: 구버전 마스터가 `DataLen ≥ 10`을 보내는가 (§10.2)
- **B3**: `Sent STDBY ACK to remote control` 로그가 홈 완료 시점에만 뜨는가 (§4.1 실장 검증)
- **B4**: **돌봄케어 모드 중 홈 완료 시 Path B로 정상 복귀하는가** (§4.1 케어 예외)

**별도 이슈**

- 리모컨 RX CRC 검증(`#if 0`) 활성화. 활성화 시 마스터 CRC의 9회 시프트와 완전 일치 확인 선행
- [`code/user_task.c:800`](../code/user_task.c#L800) `REMO_STDBY_OFF` 타임아웃 6000 → 1100
- `bed_state_lock_remain = 100` 주석("2초/3초")이 실제 10초와 불일치 — 주석 정리 (동작은 무해)
- payload 확장 시 진행 시간 필드 추가 (offset [10-11]). `reserved`에 욱여넣지 말 것

**해결됨**

- ~~Q1: watchdog 복구 시 `run_state`를 0으로 되돌리는가~~ → 예 (§5.4)
- ~~Q2: `ActionEcho`가 홈 완료 외 요인으로도 발생하는가~~ → 아니오. 단 `BedActionCare` 예외 발견 (§4.1)
- ~~Q3: `BedActionFinalize` 중 `current_mode`~~ → 마스터 v3.2.8에서 처리 (옵션 A)

---

## 요약 — 리모컨 담당자가 할 일

| # | 작업 | 절 |
|---|---|---|
| 1 | 부팅 후 `GetBedStatus(0x10, 0xF1)` 송신, 200ms × 3 재시도, 실패 시 fallback | §3 |
| 2 | `BedStatus` offset [8] `startup_pending` 파싱 | §4 |
| 3 | **전원키 첫 누름 시** `pending==1`이면 `CMD2_PWR_ON` 억제 + 확인창 표시 | §5.1 |
| 4 | 확인창 3초 long-press, 표시 중 전원키 숏클릭 차단 | §5.2.1, §6 |
| 5 | 확인 시 `stdby_timeout=1100` + `stdby_in_progress=true` + `CMD2_STDBY(0xF0)` 송신 | §5.2 |
| 6 | 완료 감지 Path A(ACK) + **Path B(`run_state 3→0`, `pending==0` 가드, `stdby_in_progress` 가드 없음)** | §5.3 |
| 7 | `pending 0→1` 전이 시 확인창 재표시 (Path B보다 우선 평가) | §5.4 |
| 8 | 통신 끊김 4.5초 감지 | §7 |
| 9 | [`drive/key.c:225`](../drive/key.c#L225) `300 → 1100` | §8 |
| 10 | tx hex 로그 | §12 |
