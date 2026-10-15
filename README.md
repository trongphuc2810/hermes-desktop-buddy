# Hermes Desktop Buddy (ESP32-S3 2.8" Display ES3C28P)

Firmware trợ lý để bàn vật lý dành cho bo mạch **2.8inch ESP32-S3 Display (ES3C28P / ES3N28P)**, tích hợp nhân đồ họa Procedural Pixel-Art Avatar của Meta (11 chế độ cảm xúc), chip âm thanh ES8311 Codec và khả năng tự động cập nhật qua GitHub Releases (OTA).

---

## 🛠️ Cấu hình phần cứng
- **MCU:** ESP32-S3-WROOM-1 (N16R8: 16MB Flash, 8MB Octal PSRAM)
- **Màn hình:** 2.8" IPS TFT 240x320 (Driver ILI9341V qua 4-line SPI)
  - `CS`: IO10, `DC`: IO46, `SCLK`: IO12, `MOSI`: IO11, `MISO`: IO13, `BL`: IO45
- **Cảm ứng:** Điện dung FT6336G (I2C SDA: IO16, SCL: IO15, INT: IO17)
- **Âm thanh:** Chip Codec ES8311 + Amp FM8002E (2W) + Micro MEMS LMA2718
  - `PA Enable`: IO1 (Active LOW), `MCLK`: IO4, `BCLK`: IO5, `LRCK`: IO7, `DOUT`: IO8, `DIN`: IO6
- **Đèn RGB:** WS2812B (IO42)
- **Nút bấm:** BOOT (IO0)

---

## ✨ Tính năng nổi bật
1. **Procedural Pixel Avatar Engine:**
   - Hoàn toàn vẽ bằng toán học số nguyên cố định Q12 (không tốn Flash lưu ảnh bitmap/GIF).
   - Tốc độ render cực mượt 25 FPS qua thư viện đồ họa LovyanGFX DMA.
   - Hỗ trợ 11 trạng thái: `BOOT`, `IDLE`, `LISTENING`, `THINKING`, `SPEAKING`, `HAPPY`, `ERROR`, `OFF`, `SLEEPING` (bay chữ Zzz), `BORED` (thở dài ra mây), `LAPTOP` (gõ phím máy tính cyber), `EATING` (uống trà sữa trân châu).
2. **ES8311 Audio Subsystem:**
   - Điều khiển Codec I2C + luồng I2S Full-Duplex (thu mic 16kHz, phát loa 16kHz).
3. **GitHub OTA Updates:**
   - Tự động kiểm tra bản cập nhật mới nhất từ GitHub Releases (`releases/latest`).
   - Tự động bám theo redirect AWS S3 CDN và flash trực tiếp qua Wi-Fi.

---

## 🚀 Hướng dẫn nạp lần đầu qua WebSerial
1. Mở trình duyệt Chrome/Edge: [Espressif Web Flasher](https://espressif.github.io/esptool-js/)
2. Cắm cáp Type-C vào máy tính, bấm **Connect**.
3. Chọn file `hermes-desktop-buddy.ino.merged.bin` trong phần Releases, đặt Flash Address: `0x0`.
4. Bấm **Program** để nạp!
