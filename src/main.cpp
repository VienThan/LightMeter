#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <BH1750.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);
BH1750 lightMeter;

// --- BỘ LỌC TRUNG BÌNH TRƯỢT ---
const int WINDOW_SIZE = 10;
float readings[WINDOW_SIZE];
int readIndex = 0;
float total = 0;
float averageLux = 0;

// --- QUẢN LÝ THỜI GIAN ---
unsigned long lastSensorReadTime = 0;
unsigned long lastLcdUpdateTime = 0;

// * LƯU Ý KHI DÙNG NẮP BẢO VỆ *
// Khi tăng MTreg, thời gian đo cần thiết sẽ tăng lên.
// Ở chế độ HighResMode2 + MTreg 200, thời gian đo khoảng ~500ms.
// Chúng ta đặt khoảng thời gian đọc cảm biến là 600ms để an toàn.
const unsigned long sensorReadInterval = 600; 
const unsigned long lcdUpdateInterval = 1000;

// * CẤU HÌNH HỆ SỐ NẮP ĐẬY (CALIBRATION FACTOR) *
// 1.0 = Không dùng nắp.
// Nếu nắp cản 50% ánh sáng, hệ số này nên là 2.0.
// Xem Phương pháp 2 bên dưới để biết cách tìm con số chính xác.
const float CAP_CORRECTION_FACTOR = 2.45; 

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("--- BAT DAU V1.2 (CAP CORRECT) ---");

  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  
  lcd.setCursor(0, 0);
  lcd.print("LightMeter");
  lcd.setCursor(0, 1);
  lcd.print("Khoi dong...");
  delay(1500);

  // 1. Khởi tạo cảm biến với độ phân giải cao nhất
  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE_2)) {
    Serial.println(F("BH1750 initialized."));

    // 2. TĂNG ĐỘ NHẠY PHẦN CỨNG (MTreg) ĐỂ BÙ ĐẮP NẮP ĐẬY 
    // Giá trị mặc định là 69. Range: 31 - 254.
    // Nếu nắp mờ vừa phải, thử 138 (gấp đôi). Nếu nắp rất mờ, thử 200-250.
    // Tăng giá trị này giúp sensor đo tốt hơn trong tối qua nắp đậy.
    byte newMTreg = 138; 
    if (lightMeter.setMTreg(newMTreg)) {
      Serial.print(F("MTreg set to: "));
      Serial.println(newMTreg);
    } else {
      Serial.println(F("Error setting MTreg"));
    }

    // Làm đầy bộ lọc ban đầu
    for (int i = 0; i < WINDOW_SIZE; i++) {
      readings[i] = lightMeter.readLightLevel();
      total += readings[i];
      delay(sensorReadInterval); // Phải chờ đủ thời gian đo mới của MTreg
    }
    averageLux = total / WINDOW_SIZE;
    
  } else {
    Serial.println(F("Error BH1750"));
    lcd.clear(); lcd.print("Sensor ERROR!");
    while (1);
  }
  lcd.clear();
}

void loop() {
  unsigned long currentMillis = millis();

  // TÁC VỤ 1: ĐỌC VÀ LỌC (Mỗi 600ms)
  if (currentMillis - lastSensorReadTime >= sensorReadInterval) {
    lastSensorReadTime = currentMillis;

    float rawReading = lightMeter.readLightLevel();

    if (rawReading >= 0) {
      // * 3. BÙ SAI SỐ DO NẮP ĐẬY BẰNG PHẦN MỀM *
      float correctedReading = rawReading * CAP_CORRECTION_FACTOR;

      // Bộ lọc trung bình
      total = total - readings[readIndex];
      readings[readIndex] = correctedReading;
      total = total + readings[readIndex];
      readIndex = (readIndex + 1) % WINDOW_SIZE;
      averageLux = total / WINDOW_SIZE;
    }
  }

  // TÁC VỤ 2: HIỂN THỊ (Mỗi 1 giây)
  if (currentMillis - lastLcdUpdateTime >= lcdUpdateInterval) {
    lastLcdUpdateTime = currentMillis;

    Serial.print("Avg Lux (Corrected): ");
    Serial.println(averageLux, 1);

    lcd.setCursor(0, 0);
    lcd.print("Gia tri Lux:  ");
    lcd.setCursor(0, 1);
    lcd.print("                ");
    lcd.setCursor(0, 1);
    
    // Giới hạn hiển thị nếu quá sáng để không vỡ khung LCD
    if(averageLux > 99999.9) {
       lcd.print("OVERLOAD");
    } else {
       lcd.print(averageLux, 1);
       lcd.print(" lux");
    }
  }
}
