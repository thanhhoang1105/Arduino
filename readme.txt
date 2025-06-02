Bạn là một AI hỗ trợ viết code Arduino/C++ cho ESP32 để tạo một hệ thống tưới vườn cà phê hoàn chỉnh với các chức năng sau. Hãy đảm bảo code chạy ổn định, không lỗi biên dịch và tuân thủ các nguyên tắc về đặt tên biến/hàm sao cho rõ ràng, dễ bảo trì. Nếu thấy bất kỳ tên biến hoặc hàm nào chưa đủ ý nghĩa, hãy tự động đổi thành tên phù hợp hơn.

1. PHẦN CỨNG (Hardware Mapping)
    1.ESP32 (38 chân)
        Có tổng cộng 12 relay (10 van + 2 bơm) được gắn vào:
		Relay 1  (Van 1)  → GPIO  2
		Relay 2  (Van 2)  → GPIO  4
		Relay 3  (Van 3)  → GPIO 16
		Relay 4  (Van 4)  → GPIO 17
		Relay 5  (Van 5)  → GPIO  5
		Relay 6  (Van 6)  → GPIO 18
		Relay 7  (Van 7)  → GPIO 19
		Relay 8  (Van 8)  → GPIO 21
		Relay 9  (Van 9)  → GPIO 22
		Relay 10 (Van 10) → GPIO 23
		Relay 11 (Bơm 1) → GPIO 26
		Relay 12 (Bơm 2) → GPIO 25

    2.IR Receiver
	    Chân DATA nối vào GPIO 15.

    3.LCD 20×4 qua I²C (PCF8574)
	    Chân SDA → GPIO 13
	    Chân SCL → GPIO 14

    4.ACS712 (Đo dòng điện)
	    Chân OUT (analog) nối vào GPIO 34.

    5.Blynk (Điều khiển từ xa qua App)
        Sử dụng Blynk để tạo 12 switch trên dashboard (tương ứng 10 van + 2 bơm).
        Mỗi khi switch trên Blynk thay đổi, phải gọi callback để cập nhật workingValveState[] hoặc workingPumpState[]. Ngược lại, khi relay thực thay đổi (qua IR hoặc menu), phải đẩy trạng thái đó lên Blynk để đồng bộ (hai chiều – hardware và Blynk giống nhau).
        Blynk kết nối qua WiFi, dùng thư viện BlynkSimpleEsp32.h, cấu hình Auth Token, SSID, Password.

2. IR CODES (Đã debug)
	#define IR_CODE_1      0xBA45FF00UL
	#define IR_CODE_2      0xB946FF00UL
	#define IR_CODE_3      0xB847FF00UL
	#define IR_CODE_4      0xBB44FF00UL
	#define IR_CODE_5      0xBF40FF00UL
	#define IR_CODE_6      0xBC43FF00UL
	#define IR_CODE_7      0xF807FF00UL
	#define IR_CODE_8      0xEA15FF00UL
	#define IR_CODE_9      0xF609FF00UL
	#define IR_CODE_0      0xE619FF00UL

	#define IR_CODE_STAR   0xE916FF00UL  // CANCEL
	#define IR_CODE_HASH   0xF20DFF00UL  // SAVE

	#define IR_CODE_UP     0xE718FF00UL
	#define IR_CODE_DOWN   0xAD52FF00UL
	#define IR_CODE_RIGHT  0xA55AFF00UL
	#define IR_CODE_LEFT   0xF708FF00UL
	#define IR_CODE_OK     0xE31CFF00UL

3. GIAO DIỆN TRÊN LCD & CÁC MENU
    3.1 MAIN_MENU (Màn hình chính)
        Khi ESP32 khởi động, LCD 20×4 hiển thị MAIN_MENU với cấu trúc:
            MAIN MENU
            > CẤU HÌNH
            THỦ CÔNG
            TỰ ĐỘNG (OFF - 1)
        Dòng 1 (row 0): tiêu đề “MAIN MENU” hoặc “MENU CHÍNH”.
        Dòng 2–4 (row 1–3): ba lựa chọn “CẤU HÌNH”, “THỦ CÔNG”, “TỰ ĐỘNG (OFF - 1)”
        “(OFF - 1)” nghĩa là chế độ AUTO đang OFF, van bắt đầu = 1.
        Con trỏ “>” chỉ mục hiện tại.
        Phím IR UP / DOWN di chuyển con trỏ, IR OK để vào submenu tương ứng.

    3.2 CONFIG_MENU (Dòng 1 của MAIN_MENU)
        Tiêu đề: “CẤU HÌNH” (hoặc “CONFIG MENU”).
        Bên trong có 3 mode (configMode = 0, 1, 2).
            Dùng IR LEFT / RIGHT để chuyển giữa mode, IR UP / DOWN để thao tác với mục con trong mỗi mode.
        IR_HASH (#) → Lưu toàn bộ thay đổi (ghi working… vào saved… qua Preferences), gọi updateOutputsFromWorking() để đồng bộ relay & Blynk, rồi quay về MAIN_MENU.
        IR_STAR (*) → Hủy: revert working… ← saved…, gọi updateOutputsFromWorking(), quay về MAIN_MENU.

	3.2.1 Mode 0 – ON/OFF cho 10 relay (10 van)
		1.Tiêu đề con (row 0–1):
			Mode 0: ON/OFF VAN
		2.Hiển thị tối đa 3 mục/trang (do LCD 4 dòng):
			> BEC 1: [ON ]
  			  BEC 2: [OFF]
  			  BEC 3: [ON ]
            “BEC i” tương ứng relay Van i (i = 1..10).
            Nếu số item > 3 (i.e. i = 1..10), hiển thị mũi tên “^” ở (row 1, col 18) nếu trang trước còn van, và “v” ở (row 3, col 18) nếu trang sau còn van.
		3.IR_UP / IR_DOWN di chuyển con trỏ (row 1–3 trong trang hiện tại).
        4.IR_OK → toggle workingValveState[currentIndex] ON ↔ OFF (chỉ cập nhật mảng tạm working…).
		5.IR_HASH (#) → commit: lưu toàn bộ workingValveState[] vào Preferences → savedValveState[], gọi updateOutputsFromWorking() để bật/tắt relay và đồng bộ Blynk, về MAIN_MENU.
        6.IR_STAR (*) → hủy: revert workingValveState[] = savedValveState[], gọi updateOutputsFromWorking(), về MAIN_MENU.
        CHÚ Ý BẢO VỆ BƠM: Luôn phải đảm bảo không có van nào mở mới được phép bật bơm. Do đó, trong updateOutputsFromWorking(), nếu phát hiện workingPumpState[x] == ON mà tất cả workingValveState[i] == OFF (i = 0..9), hãy ép workingPumpState[x] = OFF và cập nhật ngược lại cho Blynk.

	3.2.2 Mode 1 – Cấu hình “Delay” cho tất cả các van
		1.Tiêu đề con:
			Mode 1: DELAY VAN
		2.Giao diện:
			DELAY: 10s
            workingDelaySec (uint16_t) áp dụng cho tất cả van. Khi thời gian trôi hết, van cũ đóng, van tiếp theo mở, đếm ngược từng giây.
            IR_UP / IR_DOWN tăng/giảm workingDelaySec (đơn vị giây, min = 1).
		3.IR_HASH → commit: lưu workingDelaySec → Preferences, về MAIN_MENU.
		4.IR_STAR → revert workingDelaySec = savedDelaySec, về MAIN_MENU.

	3.2.3 Mode 2 – Bật/tắt ACS712 (POWER CHECK)
		1.Tiêu đề con (row 0–1):
			Mode 2: POWER CHECK
			POWER CHECK: ON
			Lấy từ workingPowerCheckEnabled.
		2.IR_OK → toggle workingPowerCheckEnabled.
		3.IR_HASH → commit: lưu workingPowerCheckEnabled → Preferences, về MAIN_MENU.
		4.IR_STAR → revert workingPowerCheckEnabled = savedPowerCheckEnabled, về MAIN_MENU.
        Bảo vệ nguồn: Nếu workingPowerCheckEnabled == true, trong loop() phải thường xuyên gọi readACS712Current() để đo dòng. Nếu dòng < 0.1 A → tắt tất cả relay và hiển thị cảnh báo “MẤT NGUỒN – CHECK POWER” trên LCD, rồi về MAIN_MENU.

    3.3 MANUAL_MENU (Dòng 2 của MAIN_MENU)
	Tiêu đề:
		THU CONG
	Giao diện chỉnh giờ phút (row 1):
		[HH:MM]
		manualHour từ 0..99, manualMin từ 0..59.
		Nếu editingHour = true, “HH” nhấp nháy; IR UP / DOWN điều chỉnh giờ +1/−1.
		Nếu editingHour = false, “MM” nhấp nháy; IR UP / DOWN tăng/giảm 15 phút (nếu < 15 và bấm DOWN → 0; nếu > 45 và bấm UP → 0).
		IR_OK → chuyển editingHour = !editingHour.
		IR_HASH (#) → lưu manualHour, manualMin vào biến toàn cục hoặc thiết lập một timer (ví dụ: nextManualMillis = millis() + manualHour*3600000 + manualMin*60000), về MAIN_MENU.
		IR_STAR (*) → hủy (không lưu), về MAIN_MENU.

        Tính năng bổ sung:
        Có thể cho phép nhập trực tiếp “HH” qua bàn phím số IR (nhấn các phím số 0–9) để nhập nhanh giờ nếu muốn.
        Khi đến thời điểm manualHour:manualMin, vòng lặp loop() kiểm tra bằng if (nowMatchesManualTime()) để bật các van theo savedValveState[], cho phép tưới từng van, rồi tự động tắt sau khoảng thời gian “Delay” đã cài. Bạn có thể cài đặt một biến isManualRunning = true để quản lý chu kỳ.

    3.4 AUTO_MENU (Dòng 3 của MAIN_MENU)
	Tiêu đề (row 0–1):
		TU DONG (ON - X)   // nếu autoEnabled == true
        TU DONG (OFF - X)  // nếu autoEnabled == false
		autoEnabled (bool) = ON/OFF, autoStartIndex = X (1..10).
	Sub-menu (row 1–3) gồm 5 mục:
	1.AUTO_ON:
		IR_OK → set autoEnabled = true, nếu chưa có autoStartIndex thì mặc định = 1, về MAIN_MENU (hiển thị “TU DONG (ON - X)”).

	2.AUTO_OFF:
		IR_OK → set autoEnabled = false, về MAIN_MENU (“TU DONG (OFF - X)”).

	3.AUTO_RESET:
		IR_OK → set autoEnabled = false; autoStartIndex = 1;, về MAIN_MENU.

	4.CurrentSprinkler: chỉnh “van bắt đầu”
		Giao diện: Chon VAN: [X]
		IR_UP / IR_DOWN tăng/giảm workingAutoStartIndex (1..10).
		IR_HASH (#) → commit: autoStartIndex = workingAutoStartIndex; về AUTO_MENU. Nếu autoEnabled == true, lúc này hiện “TU DONG (ON - autoStartIndex)”.
		IR_STAR (*) → revert workingAutoStartIndex = savedAutoStartIndex, về AUTO_MENU.

	5.TIME_AUTO_SELECT: đặt lịch tự động
		Chia thành hai phần:
			1.FROM – TO:
				F[HH:MM] - T[HH:MM]
			IR_OK chuyển giữa 4 trường chỉnh: F giờ, F phút, T giờ, T phút.
			IR_UP / IR_DOWN tăng/giảm 1 đơn vị (giờ: 0..23, phút: 0..59).

			2.DURATION – REST:
				D[HH:MM] - R[HH:MM]
			IR_OK chuyển giữa D giờ, D phút, R giờ, R phút.
			IR_UP / IR_DOWN tăng/giảm 1 đơn vị (giờ: 0..23, phút: 0..59).
			IR_HASH (#) → commit:
                savedAutoFrom   = workingAutoFrom;
                savedAutoTo     = workingAutoTo;
                savedDuration   = workingDuration;
                savedRest       = workingRest;
                Lưu vào Preferences, về AUTO_MENU.
			IR_STAR (*) → revert:
                workingAutoFrom   = savedAutoFrom;
                workingAutoTo     = savedAutoTo;
                workingDuration   = savedDuration;
                workingRest       = savedRest;
                Về AUTO_MENU.

            Cơ chế tự động (nếu muốn thực thi):
            Trong loop(), nếu autoEnabled == true, kiểm tra thời gian hiện tại (có thể dùng RTC hoặc millis() với offset).
            Nếu giờ hiện tại nằm trong khoảng [savedAutoFrom, savedAutoTo], thực hiện quy trình tưới:
                1.Bật savedValveState[autoStartIndex - 1] (relay thứ autoStartIndex) → tắt sau savedDuration.
                2.Sau đó chờ thêm savedRest, rồi tăng autoStartIndex++, nếu > 10 → về 1, lặp lại.
            Nếu ra ngoài khung FROM – TO, tắt hết relay.

4. YÊU CẦU TỔNG QUÁT
    1.Đặt tên biến/hàm rõ ràng
    – Ví dụ:
        bool   savedValveState[10],   workingValveState[10];
        uint16_t savedDelaySec,       workingDelaySec;
        bool   savedPowerCheckEnabled, workingPowerCheckEnabled;

        bool   autoEnabled;
        uint8_t savedAutoStartIndex,  workingAutoStartIndex;
        struct TimeHM { uint8_t hour, minute; };
        TimeHM savedAutoFrom, workingAutoFrom, savedAutoTo, workingAutoTo;
        TimeHM savedDuration, workingDuration, savedRest, workingRest;

        uint8_t manualHour, manualMin;
        bool    editingManualHour;
    Nếu thấy tên chưa rõ, hãy đổi thành tên mô tả chức năng.

    2.Quản lý trạng thái
        – Sử dụng enum AppState { STATE_MAIN, STATE_CONFIG, STATE_MANUAL, STATE_AUTO }
        – Sử dụng enum ConfigMode { MODE_ONOFF, MODE_DELAY, MODE_POWERCHECK }
        – Giữ mảng working… để thao tác tạm, mảng saved… để lưu vào Preferences.

    3.Preferences (ROM)
        – Gọi prefs.begin("cfg", false); trong setup().
        – Hàm loadConfigFromPrefs() đọc:
        • for i in 0..9: savedValveState[i] = prefs.getBool("v"+i, false);
        • savedDelaySec = prefs.getUInt("delay", 5);
        • savedPowerCheckEnabled = prefs.getBool("power", false);
        • savedAutoStartIndex = prefs.getUInt("as", 1);
        • savedAutoFrom…, savedAutoTo…, savedDuration…, savedRest… lần lượt.
        – Hàm saveConfigToPrefs() ngược lại ghi working… vào Prefs.
        – Mỗi khi bấm IR_HASH, gọi saveConfigToPrefs().

    4.IRQ/IR processing
        – Trong loop(), gọi if (IrReceiver.decode()) { code = IrReceiver.decodedIRData.decodedRawData; handleIR(code); IrReceiver.resume(); }.
        – handleIR(unsigned long code) xử lý theo appState và IR code (UP, DOWN, LEFT, RIGHT, OK, HASH, STAR).

    5.Đồng bộ với Blynk
        – Kết nối WiFi và khởi tạo Blynk với Auth Token.
        – Định nghĩa 12 Virtual Pins (0..11) tương ứng 10 van và 2 bơm.
        BLYNK_WRITE(V0) { workingValveState[0] = param.asInt(); updateOutputsFromWorking(); }
        ...
        BLYNK_WRITE(V9) { workingValveState[9] = param.asInt(); updateOutputsFromWorking(); }
        BLYNK_WRITE(V10){ workingPumpState[0]  = param.asInt(); updateOutputsFromWorking(); }
        BLYNK_WRITE(V11){ workingPumpState[1]  = param.asInt(); updateOutputsFromWorking(); }
        – Trong updateOutputsFromWorking(), sau khi viết ra GPIO, hãy đẩy trạng thái này ngược vào Blynk bằng Blynk.virtualWrite(Vn, value); để đồng bộ hai chiều.
        – Bảo vệ bơm: trước khi cho phép bật bơm, phải kiểm tra if (anyVanOpen()) (ít nhất một workingValveState[i] == true). Nếu không có van nào mở, ép workingPumpState[j] = false, cập nhật Blynk về “OFF”, và không cho bật bơm.

    6.Relay & Bảo vệ ACS712
        – Trong updateOutputsFromWorking():
        for (int i = 0; i < 10; i++) {
            digitalWrite(pinRelayVan[i], workingValveState[i] ? LOW : HIGH);
            Blynk.virtualWrite(Vi, workingValveState[i] ? 1 : 0);
            }
            // Bơm:
            bool anyOpen = false;
            for (int i = 0; i < 10; i++) if (workingValveState[i]) { anyOpen = true; break; }
            for (int j = 0; j < 2; j++) {
            if (!anyOpen) workingPumpState[j] = false;
            digitalWrite(pinRelayPump[j], workingPumpState[j] ? LOW : HIGH);
            Blynk.virtualWrite(V10 + j, workingPumpState[j] ? 1 : 0);
        }
        – Nếu workingPowerCheckEnabled == true, trong loop() phải đo current = readACS712Current(). Nếu current < 0.1 → gọi turnAllOff() và hiển thị “MẤT NGUỒN” trên LCD, đồng thời Blynk cập nhật OFF cho tất cả.

    7.Manual & Auto Logic
        – Manual: Khi bấm # ở MANUAL_MENU, lưu manualHour, manualMin. Trong loop(), so sánh thời gian (có thể dùng RTC hoặc millis() kết hợp offset), khi đến manualHour:manualMin, thực hiện tưới theo savedValveState[] (ở chế độ ON của van), bật tất cả van đã chọn, đợi savedDelaySec, rồi tắt tất cả.
        – Auto: Nếu autoEnabled == true, trong loop() liên tục kiểm tra if (nowBetween(savedAutoFrom, savedAutoTo)). Nếu đúng, bắt đầu chu trình:

        1.Bật van autoStartIndex (và đóng các van khác), đợi savedDuration.
        2.Tắt van đó, đợi savedRest.
        3.Tăng autoStartIndex++, if > 10 thì reset về 1.
        4.Lặp lại cho đến khi ra khỏi khung giờ.
            – Khi ra khỏi khung [FROM, TO], tắt hết van.

5.YÊU CẦU
    1.Đặt tên biến/hàm rõ ràng, mô tả đúng chức năng. Nếu tên ban đầu chưa đủ ý nghĩa, AI phải tự động đổi thành tên tốt hơn.
    2.Phân tách mảng “working” / “saved” cho:
        – Trạng thái mở/tắt van (10 phần tử), bơm (2 phần tử),
        – Delay chung (uint16_t),
        – PowerCheck (bool),
        – Auto: autoEnabled, autoStartIndex, autoFrom, autoTo, autoDuration, autoRest,
        – Manual: manualHour, manualMin, editingManualHour
    3.Menu điều khiển qua IR
    4.Menu hiển thị LCD
    5.Blynk đồng bộ (Hai chiều)
    6.Bảo vệ bơm
    7.Preferences
    8.Logic chính (loop)
        – Gọi Blynk.run(); để giữ kết nối Blynk.
        – Xử lý IR: if (IrReceiver.decode()) { handleIR(code); IrReceiver.resume(); }
        – Nếu workingPowerCheckEnabled == true, gọi readACS712Current(). Nếu dòng < 0.1 A, gọi turnAllOff() (tắt van & bơm), hiển thị cảnh báo, cập nhật Blynk.
        – Kiểm tra Manual: nếu manualEnabled == true && nowMatches(manualHour, manualMin), thực hiện tưới theo savedValveState[] (bật relay), delay savedDelaySec, tắt, đặt manualEnabled = false.
        – Kiểm tra Auto: nếu autoEnabled == true && nowInRange(savedAutoFrom, savedAutoTo), thực hiện quy trình tưới tự động: bật van autoStartIndex, delay savedDuration, tắt, delay savedRest, autoStartIndex++, if > 10 → 1, lặp. Nếu ra ngoài khung giờ, tắt hết van, set autoRunning = false.

6. KẾT LUẬN
    Đây là một hệ thống tưới vườn cà phê đầy đủ, bao gồm:
    12 relay (10 van + 2 bơm) gắn vào ESP32.
    IR remote điều khiển menu, toggle van, chỉnh delay, bật/tắt ACS712, hẹn giờ thủ công, cài đặt tự động.
    LCD 20×4 hiển thị các menu đầy đủ, có con trỏ, mũi tên cuộn, hướng dẫn “# Save * Cancel”.
    ACS712 đo dòng, nếu bật chế độ POWER CHECK, tự động tắt relay khi mất nguồn.
    Blynk đồng bộ hai chiều: 12 switch trên app và 12 relay thực tế luôn khớp, với bảo vệ bơm không cho bật khi không có van mở.
    Lưu cấu hình (van, delay, PowerCheck, Auto, Manual) vào Preferences, đảm bảo qua khởi động lại vẫn giữ nguyên.
    Chỉnh giờ/phút tuân thủ quy tắc: giờ tăng/giảm ±1, phút ±15 với vòng lặp (nếu < 15 và giảm về 0, nếu > 45 và tăng về 0). Cho phép nhập trực tiếp số giờ qua IR numeric nếu muốn.
    Đặt tên biến/hàm rõ ràng, nhất quán (working… vs saved…, updateOutputsFromWorking(), handleIR(), drawMainMenu(), v.v.).

Hãy viết code đầy đủ theo yêu cầu trên, tách thành các hàm, dùng cấu trúc (enum, struct, mảng) khoa học, và đảm bảo không có lỗi biên dịch, logic chạy trơn tru trên ESP32.