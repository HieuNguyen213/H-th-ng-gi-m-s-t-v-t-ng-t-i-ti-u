#define BLYNK_TEMPLATE_ID "TMPL6Rq3G5x8N"
#define BLYNK_TEMPLATE_NAME "Project1"
#define BLYNK_AUTH_TOKEN "Hij5jQrsI2n1E2Gim9Mpg17iY_uoKjp_" // Auth Token cho Blynk

// Thư viện cần cho dựa án
#include <DHTesp.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h> // Thư viện cho giao thức I2C
#include <Adafruit_GFX.h> // Thư viện đồ họa cho màn hình OLED
#include <Adafruit_SSD1306.h> // Thư viện cho màn hình OLED SSD1306
#include <WiFi.h>
#include <NTPClient.h>

// Thiết lập các chân
#define SOILMOISTURE_PIN 35     // Chân độ ẩm đất
#define OLED_SDA 21
#define OLED_SCL 22
#define DHTPIN 15 // Chân cảm biến DHT21
#define LED_PIN 12 // Chân GPIO cho LED
#define RELAY_PIN 14 // Chân GPIO cho relay
#define SWITCH_VIRTUAL_PIN 4 // Chân ảo nhận tín hiệu từ Blynk
#define BLYNK_LED_VIRTUAL_PIN 3 // Chân ảo của LED trong Blynk
#define CHECK_INTERVAL 900000  // 15 phút gọi hàm 1 lần
#define THRESHOLD_TEMP 35 // Ngưỡng nhiệt độ là 35 độ C

//Kết nối Blynk và wifi
char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "hung1011"; // SSID của Wi-Fi
char pass[] = "10111988"; // Mật khẩu Wi-Fi

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7 * 3600; // Offset từ GMT (Việt Nam là GMT+7)
const int daylightOffset_sec = 0; // Không có ánh sáng ban ngày (daylight saving time)
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServer, gmtOffset_sec);

BlynkTimer timer;

// Khởi tạo cảm biến nhiệt độ độ ẩm
DHTesp dht;
int hour, minute;
int dayOfWeek, month;
static float t, h, hs;

// Cấu hình màn hình OLED
#define SCREEN_WIDTH 128 // Chiều rộng màn hình (128 pixels)
#define SCREEN_HEIGHT 64 // Chiều cao màn hình (64 pixels)
#define OLED_RESET -1 // Không sử dụng chân reset
#define OLED_I2C_ADDRESS 0x3C // Địa chỉ I2C mặc định của OLED
// Khởi tạo đối tượng OLED
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET, OLED_SDA, OLED_SCL);

bool switchOfBlynk = false; // Biến cờ xác định trạng thái của switch Blynk
bool autoMode = true;

static unsigned long startTime = 0; // Biến lưu thời điểm bắt đầu quá trình tưới
const unsigned long duration = 60000; // Thời gian tưới (ms), ví dụ 1 phút
const unsigned long extraWateringDuration  = 30000; // Thời gian tưới bổ sung (30 giây)
static unsigned long temp = 0;
static bool watering = false;

// Biến lưu thời gian lần cuối cùng hàm checkTemperature() được gọi
static unsigned long lastCheckTime = 0;
static unsigned long currentTime = 0;
unsigned int count = 0; // Biến đếm

void turnOn(){
  digitalWrite(RELAY_PIN, HIGH); // Bật relay
  digitalWrite(LED_PIN, HIGH); // Bật LED của hệ thống
  Blynk.virtualWrite(V3, 255); // Bật LED của Blynk(với giá trị 255)
}

void turnOff(){
  digitalWrite(RELAY_PIN, LOW); // Tắt relay
  digitalWrite(LED_PIN, LOW); // Tắt LED của hệ thống
  Blynk.virtualWrite(V3, 0); // Tắt LED của Blynk(với giá trị 0)
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12); // Đặt độ phân giải ADC là 12 bit (0-4095)
  dht.setup(DHTPIN, DHTesp::DHT11); // Khởi động cảm biến DHT11
  Serial.println("Bat dau ket noi");
  Blynk.begin(auth, ssid, pass); // Kết nối đến Blynk bằng wifi đã thiết lập ở trên
  while (WiFi.status() != WL_CONNECTED) {
        delay(1000); // Đợi 1 giây
        Serial.println("Connecting...");
  }
  // In địa chỉ IP sau khi kết nối thành công
    Serial.println("Kết nối WiFi thành công");
    Serial.print("Địa chỉ IP: ");
    Serial.println(WiFi.localIP());
  timer.setInterval(2000L, readData); // Thiết lập thời gian gửi dữ liệu (timer.run()), 2 giây/lần
  timer.setInterval(2000L, sendData); // Thiết lập thời gian gửi dữ liệu (2 giây/lần)
  timer.setInterval(2000L, displayOLED); // Thiết lập thời gian cập nhật OLED (2 giây/lần)
  timer.setInterval(2000L, mode); // Thiết lập thời gian kiểm tra chế độ (2 giây/lần)
  Wire.begin(OLED_SDA, OLED_SCL);
  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS)) {
    while (true); // Dừng lại nếu không thể khởi động màn hình OLED
  }
  timeClient.begin();
  pinMode(LED_PIN, OUTPUT); // Thiết lập LED là đầu ra
  pinMode(RELAY_PIN, OUTPUT); // Thiết lập chân relay là đầu ra
  turnOff();
}

void readData(){
  t = dht.getTemperature(); // Đọc nhiệt độ rồi lưu vào biến t
  h = dht.getHumidity(); // Đọc độ ẩm rồi lưu vào biến h
  hs = analogRead(SOILMOISTURE_PIN);    //đọc giá trị analog từ cảm biến độ ẩm đất rồi lưu vào hs
  hs = map(hs,0,4095,100,0);  //đổi giá trị sang từ 0 -> 100
  Serial.println("Nhiet do:" + String(t));
  Serial.println("do am:" + String(h));
  Serial.println("do am dat:" + String(hs));
}

void sendData(){
 // readData();
  //Đọc xong thì gửi dữ liệu vào các chân ảo tương ứng trong Blynk
  Blynk.virtualWrite(V0, t); // Gửi dữ liệu nhiệt độ từ biến t đến Blynk
  Blynk.virtualWrite(V1, h); // Gửi dữ liệu độ ẩm từ biến h đến Blynk
  Blynk.virtualWrite(V2, hs); // Gửi dữ liệu độ ẩm đất từ biến hs đến Blynk
}

void displayOLED() {
  // Kiểm tra nếu dữ liệu hợp lệ thì hiển thị lên OLED
  if (!isnan(t) && !isnan(h) && !isnan(hs)) {
    // Xóa màn hình trước khi hiển thị thông tin mới
    oled.clearDisplay();
    // Hiển thị nhiệt độ và độ ẩm trên màn hình OLED
    oled.setTextSize(1); // Chọn kích thước chữ
    oled.setTextColor(SSD1306_WHITE); // Chọn màu chữ (trắng)
    oled.setCursor(0, 0); // Đặt con trỏ để viết văn bản
    oled.println("Nhom 20 - Do an 1");
    oled.setCursor(0, 12); // Chuyển con trỏ xuống
    oled.println("Nhiet do: " + String(t) + "C"); // Hiển thị nhiệt độ
    oled.setCursor(0, 24); // Chuyển con trỏ xuống
    oled.println("Do am: " + String(h) + "%"); // Hiển thị độ ẩm
    oled.setCursor(0, 36); // Chuyển con trỏ xuống
    oled.println("Do am dat: " + String(hs) + "%"); // Hiển thị độ ẩm đất
    oled.display(); // Cập nhật màn hình với thông tin mới
  }
}

// Hàm đọc tín hiệu từ Blynk
BLYNK_WRITE(SWITCH_VIRTUAL_PIN) { // Chân ảo nhận tín hiệu từ Blynk
  int state = param.asInt(); // Lấy trạng thái từ Blynk (0 hoặc 1)
  if(state == 1) {
    switchOfBlynk = true;
    autoMode = false;
  }
  else{
    switchOfBlynk = false;
  }
}

//Cài đặt chế độ tưới, tưới tự động và tưới thủ công thông qua điều khiển trên Blynk
void mode(){
  // Lấy thời gian hiện tại (theo múi giờ của Việt Nam)
  String formattedTime = timeClient.getFormattedTime();
  // Lấy ngày hiện tại (theo múi giờ của Việt Nam)
  time_t epochTime = timeClient.getEpochTime();
  struct tm *timeInfo;
  timeInfo = localtime(&epochTime);
  // Lấy giờ và phút
  hour = timeInfo->tm_hour;
  minute = timeInfo->tm_min;
  // Lấy ngày trong tuần (0: Chủ nhật, 1: Thứ 2, ..., 6: Thứ 7)
  dayOfWeek = timeInfo->tm_wday;
  // Lấy tháng (0: Tháng 1, 1: Tháng 2, ..., 11: Tháng 12)
  month = timeInfo->tm_mon + 1; // Thêm 1 vì tháng trong cấu trúc tm là từ 0 đến 11
  // Kiểm tra điều kiện, nếu switchOfBlynk == false tức là nút tưới thủ công trên Blynk chưa được bấm
  Serial.print("month: ");
  Serial.println(month);
  Serial.print("day: ");
  Serial.println(dayOfWeek);
  Serial.print("hour: ");
  Serial.println(hour);
  Serial.print("minute: ");
  Serial.println(minute);
  if(autoMode){ // Chế độ tưới tự động
    if((hour == 10) && ((minute >= 29) && (minute < 22))){
      if(!watering){
        startTime = millis();
        watering = true;
        turnOn();
      }
    } 
    temp = millis() - startTime;
    if ((watering) && (temp >= duration)) {
      Serial.println("Off");
      Serial.println(temp);
      turnOff(); // Tắt thiết bị
      watering = false;
      temp = 0;
    }
  }
  else{ // Chế độ tưới thủ công
    if(switchOfBlynk) turnOn();
    else{
      turnOff();
      autoMode = true;
    }
  }
}

void checkTemperature() {
  if (t > THRESHOLD_TEMP) { // Nếu nhiệt độ vượt quá ngưỡng
    count++; // Tăng biến đếm
    Serial.println(count);
    Serial.println("\n");
  }
}

int dayWateringNeeded = 0; 

void irrigate(){
  if(count >= 15){
    dayWateringNeeded = dayOfWeek + 1;
  }
  if(dayWateringNeeded != 0){
    if((hour == 10) && ((minute >= 29) && (minute < 22)) && (dayOfWeek == dayWateringNeeded)){
      if(!watering){
        startTime = millis();
        watering = true;
        turnOn();
        dayWateringNeeded = 0;
      }
    }
  }
  temp = millis() - startTime;
  if ((watering) && (temp >= extraWateringDuration)) {
    Serial.println("Off");
    turnOff(); // Tắt thiết bị
    watering = false;
    temp = 0;
  }
}

void loop() {
  currentTime = millis(); // Lấy thời gian hiện tại
  timeClient.update();
  mode();
  if ((currentTime - lastCheckTime) >= CHECK_INTERVAL) {
    if (hour >= 11 && hour <= 16){
      checkTemperature(); // Gọi hàm kiểm tra nhiệt độ
      lastCheckTime = currentTime; // Cập nhật thời gian cuối cùng
    }
  }
  irrigate();
  Blynk.run(); // Vòng lặp chính của Blynk (Duy trì kết nối giữa hệ thống với Blynk)
  timer.run(); // Thực hiện nhiệm vụ mỗi giây 2 lần
}