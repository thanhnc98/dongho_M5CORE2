// Trang cau hinh mo bang trinh duyet dien thoai (cung mang WiFi voi dong ho).
// Anh nen duoc trinh duyet cat/thu nho ve 320x240, lam toi va doi sang RGB565 truoc khi gui.
#pragma once
#include <Arduino.h>

static const char WEB_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="vi">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Đồng hồ M5Stack</title>
<style>
  :root { color-scheme: dark; }
  * { box-sizing: border-box; }
  body { margin: 0; padding: 16px; background: #0d1321; color: #e8ecf4;
         font: 16px/1.4 system-ui, -apple-system, "Segoe UI", Roboto, sans-serif; }
  main { max-width: 420px; margin: 0 auto; }
  h1 { font-size: 20px; margin: 4px 0 16px; }
  h2 { font-size: 16px; margin: 0 0 12px; color: #5cc8ff; }
  section { background: #182036; border-radius: 12px; padding: 16px; margin-bottom: 16px; }
  .row { display: flex; align-items: center; justify-content: space-between; gap: 12px; margin: 10px 0; }
  .row span { flex: 1; }
  input[type=time], input[type=number], input[type=text], input[type=password] {
         width: 120px; padding: 8px; border-radius: 8px;
         border: 1px solid #33405f; background: #0d1321; color: inherit; font-size: 16px; }
  #ssid, #wpass, #locName, #locLat, #locLon { width: 190px; }
  input:disabled { opacity: .5; }
  .net { display: flex; justify-content: space-between; gap: 8px; width: 100%; margin: 4px 0; padding: 8px 10px;
         border: 0; border-radius: 8px; background: #0d1321; color: inherit; font-size: 15px; text-align: left; }
  .net small { color: #9fb0cf; }
  input[type=checkbox] { width: 24px; height: 24px; }
  .swgrid { display: grid; grid-template-columns: repeat(auto-fill, minmax(112px, 1fr)); gap: 8px; margin: 4px 0 8px; }
  .sw { display: flex; align-items: center; justify-content: flex-start; gap: 8px; min-width: 0;
        margin: 0; padding: 8px; border: 2px solid transparent; border-radius: 10px;
        background: #0d1321; color: inherit; font-size: 14px; text-align: left; }
  .sw.on { border-color: #5cc8ff; background: #111a30; }
  .sw i { flex: none; width: 22px; height: 22px; border-radius: 50%; border: 1px solid #55648a; }
  .sw span { overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
  #wheel { display: block; width: 240px; height: 240px; max-width: 100%; margin: 14px auto 10px;
           border-radius: 50%; background: transparent; touch-action: none; aspect-ratio: 1 / 1; }
  #hex { width: 96px; text-align: center; font-family: monospace; }
  input[type=range] { flex: 1.4; }
  input[type=file] { width: 100%; margin-bottom: 12px; }
  canvas { display: block; width: 100%; max-width: 320px; margin: 0 auto 12px; border-radius: 8px;
           background: #000; aspect-ratio: 4 / 3; }
  .btns { display: flex; flex-wrap: wrap; gap: 8px; margin-top: 14px; }
  button { flex: 1; min-width: 110px; padding: 12px; border: 0; border-radius: 10px;
           background: #1e90ff; color: #fff; font-size: 16px; }
  button.alt { background: #33405f; }
  button.st { min-width: 90px; border: 2px solid transparent; }
  button.st.on, button.dm.on { border-color: #5cc8ff; background: #1e4a7a; }
  button.dm { border: 2px solid transparent; }
  button:disabled { opacity: .5; }
  .msg { min-height: 1.4em; margin: 10px 0 0; font-size: 14px; color: #9fb0cf; }
  .ok { color: #6ee7a0; } .err { color: #ff8080; }
</style>
</head>
<body>
<main>
  <h1>Đồng hồ M5Stack</h1>

  <section>
    <h2>WiFi</h2>
    <p class="msg" id="wst">Đang đọc trạng thái...</p>
    <label class="row"><span>Tên WiFi</span><input type="text" id="ssid" maxlength="32" autocapitalize="off" autocomplete="off"></label>
    <div id="nets"></div>
    <label class="row"><span>Mật khẩu</span><input type="password" id="wpass" maxlength="63" autocomplete="off"></label>
    <label class="row"><span>Hiện mật khẩu</span><input type="checkbox" id="wshow"></label>
    <div class="btns">
      <button id="wsave">Lưu và kết nối</button>
      <button id="scan" class="alt">Quét WiFi</button>
    </div>
    <p class="msg" id="msg0"></p>
  </section>

  <section>
    <h2>Khu vực thời tiết</h2>
    <label class="row"><span>Tự động theo vị trí mạng</span><input type="checkbox" id="locAuto"></label>
    <label class="row"><span>Tên khu vực</span><input type="text" id="locName" maxlength="40" autocomplete="off"></label>
    <label class="row"><span>Vĩ độ</span><input type="text" id="locLat" inputmode="decimal" autocomplete="off"></label>
    <label class="row"><span>Kinh độ</span><input type="text" id="locLon" inputmode="decimal" autocomplete="off"></label>
    <div class="btns">
      <button id="locSave">Lưu khu vực</button>
      <button id="locRefresh" class="alt">Dò lại vị trí</button>
    </div>
    <p class="msg" id="msg5">Tự động dùng địa chỉ IP của mạng nên chỉ đúng ở mức thành phố, đôi khi lệch sang tỉnh bên cạnh. Nếu sai, tắt tự động và nhập tay.</p>
  </section>

  <section>
    <h2>Chế độ ngủ</h2>
    <label class="row"><span>Tự động ngủ theo giờ</span><input type="checkbox" id="en"></label>
    <label class="row"><span>Bắt đầu ngủ</span><input type="time" id="st"></label>
    <label class="row"><span>Thức dậy</span><input type="time" id="et"></label>
    <label class="row"><span>Trong giờ ngủ, tự tắt lại sau (giây)</span>
      <input type="number" id="idle" min="5" max="3600" step="5"></label>
    <div class="btns">
      <button id="save">Lưu</button>
      <button id="sleepNow" class="alt">Ngủ ngay</button>
      <button id="wake" class="alt">Đánh thức</button>
    </div>
    <p class="msg" id="msg1"></p>
  </section>

  <section>
    <h2>Âm thanh báo giờ</h2>
    <label class="row"><span>Kêu khi sang giờ mới</span><input type="checkbox" id="chOn"></label>
    <label class="row"><span>Âm lượng</span><input type="range" id="chVol" min="0" max="100" step="5" value="50"></label>
    <label class="row"><span>Im lặng khi đồng hồ đang ngủ</span><input type="checkbox" id="chQuiet"></label>
    <p class="msg" id="chInfo">Âm thanh: ...</p>
    <input type="file" id="chFile" accept="audio/*">
    <div class="btns">
      <button id="chTest" class="alt">Nghe thử</button>
      <button id="chReset" class="alt">Âm thanh mặc định</button>
    </div>
    <p class="msg" id="msg7">Phát vào đầu mỗi giờ (…:00). Tải lên file âm thanh bất kỳ (MP3, WAV, M4A, OGG...); tối đa 10 giây, dài hơn sẽ được cắt. Không phát ở màn hình nhạc vì loa dùng chung với micro.</p>
  </section>

  <section>
    <h2>Hiển thị ngày</h2>
    <div class="btns">
      <button class="alt dm" data-m="0">Dương lịch</button>
      <button class="alt dm" data-m="1">Âm lịch</button>
      <button class="alt dm" data-m="2">Tự động (10 giây)</button>
    </div>
    <p class="msg" id="msg6">Tự động: dòng ngày trên đồng hồ đổi qua lại giữa dương lịch và âm lịch mỗi 10 giây. Chạm vào dòng ngày để đổi nhanh.</p>
  </section>

  <section>
    <h2>Màn hình nhạc</h2>
    <div class="btns">
      <button id="musicOn">Bật</button>
      <button id="musicOff" class="alt">Về đồng hồ</button>
    </div>
    <p class="msg">Kiểu sóng (chạm để chọn và bật):</p>
    <div class="btns" id="styles">
      <button class="alt st" data-s="0">Cột</button>
      <button class="alt st" data-s="1">Quả cầu</button>
      <button class="alt st" data-s="2">Đối xứng</button>
      <button class="alt st" data-s="3">Vòng tròn</button>
      <button class="alt st" data-s="4">Thác phổ</button>
      <button class="alt st" data-s="5">Phi hành gia</button>
      <button class="alt st" data-s="6">Tim bay</button>
    </div>
    <p class="msg" id="msg4">Hình và LED nháy theo âm thanh từ micro. Trên đồng hồ: vuốt từ phải sang trái để bật, vuốt ngược lại để về, nút B để đổi kiểu sóng.</p>
  </section>

  <section>
    <h2>Màu chữ</h2>
    <div class="swgrid">
      <button class="sw" data-c="time"><i></i><span>Giờ phút</span></button>
      <button class="sw" data-c="colon"><i></i><span>Dấu hai chấm</span></button>
      <button class="sw" data-c="date"><i></i><span>Ngày tháng</span></button>
      <button class="sw" data-c="temp"><i></i><span>Nhiệt độ</span></button>
      <button class="sw" data-c="desc"><i></i><span>Mô tả</span></button>
      <button class="sw" data-c="label"><i></i><span>Khu vực</span></button>
      <button class="sw" data-c="value"><i></i><span>Độ ẩm, gió</span></button>
    </div>

    <canvas id="wheel" width="240" height="240"></canvas>
    <label class="row"><span>Độ sáng</span><input type="range" id="val" min="0" max="1" step="0.01" value="1"></label>
    <label class="row"><span>Mã màu</span><input type="text" id="hex" maxlength="7" value="#ffffff"></label>
    <div class="btns">
      <button id="colReset" class="alt">Màu mặc định</button>
    </div>
    <p class="msg" id="msg3">Chạm một mục ở trên, rồi kéo trên vòng tròn màu. Thả tay là đồng hồ đổi màu.</p>
  </section>

  <section>
    <h2>Ảnh nền</h2>
    <input type="file" id="file" accept="image/*">
    <canvas id="cv" width="320" height="240"></canvas>
    <label class="row"><span>Phóng to</span><input type="range" id="zoom" min="1" max="3" step="0.01" value="1"></label>
    <label class="row"><span>Trái / phải</span><input type="range" id="px" min="0" max="1" step="0.01" value="0.5"></label>
    <label class="row"><span>Trên / dưới</span><input type="range" id="py" min="0" max="1" step="0.01" value="0.5"></label>
    <label class="row"><span>Độ sáng</span><input type="range" id="br" min="0.2" max="1" step="0.01" value="0.6"></label>
    <div class="btns">
      <button id="up" disabled>Gửi ảnh lên đồng hồ</button>
      <button id="reset" class="alt">Dùng ảnh mặc định</button>
    </div>
    <p class="msg" id="msg2">Chọn ảnh, chỉnh khung và độ sáng (nên để hơi tối để chữ dễ đọc), rồi gửi.</p>
  </section>

  <section>
    <h2>Cập nhật firmware (FOTA)</h2>
    <p class="msg" id="otaInfo">Phiên bản: ...</p>
    <label class="row" id="otaKeyRow" style="display:none"><span>Mật khẩu cập nhật</span><input type="password" id="otaKey" autocomplete="off"></label>
    <input type="file" id="otaFile" accept=".bin">
    <div class="btns"><button id="otaUp">Gửi file .bin và cập nhật</button></div>
    <label class="row"><span>Hoặc tải từ URL</span><input type="text" id="otaUrl" placeholder="https://.../firmware.bin" autocapitalize="off" autocomplete="off" style="width:100%"></label>
    <label class="row"><span>MD5 (tùy chọn)</span><input type="text" id="otaMd5" maxlength="32" autocapitalize="off" autocomplete="off" style="width:240px"></label>
    <div class="btns"><button id="otaGo" class="alt">Đồng hồ tải về và cập nhật</button></div>
    <p class="msg" id="msg8">File là firmware.bin do PlatformIO tạo ra (.pio/build/m5stack-core2/firmware.bin). Trong lúc cập nhật đừng tắt nguồn; xong đồng hồ tự khởi động lại, nếu lỗi thì vẫn chạy bản cũ. Với https, nên nhập MD5 để chắc đúng file.</p>
  </section>
</main>

<script>
const $ = id => document.getElementById(id);
const W = 320, H = 240;

function say(id, text, cls) { const e = $(id); e.textContent = text; e.className = 'msg ' + (cls || ''); }

async function post(url, body) {
  const r = await fetch(url, { method: 'POST', body });
  const t = await r.text();
  if (!r.ok) throw new Error(t || ('HTTP ' + r.status));
  return t;
}

// ----- Che do ngu -----
async function loadState() {
  try {
    const s = await (await fetch('/api/state')).json();
    $('en').checked = s.enabled; $('st').value = s.start; $('et').value = s.end; $('idle').value = s.idle;
    if (s.colors) { Object.assign(colors, s.colors); refreshSwatches(); selectItem(current); }
    if (s.wifi) showWifi(s.wifi);
    if (s.music) showStyle(s.music.style);
    if (s.loc) showLoc(s.loc);
    if (s.dateMode !== undefined) showDate(s.dateMode);
    if (s.chime) showChime(s.chime);
  } catch (e) { say('msg1', 'Không đọc được cài đặt: ' + e.message, 'err'); }
}

// ----- Am thanh bao gio -----
function showChime(c) {
  $('chOn').checked = c.on; $('chVol').value = c.vol; $('chQuiet').checked = c.quiet;
  $('chVol').disabled = $('chQuiet').disabled = !c.on;
  const s = (c.ms / 1000).toFixed(1).replace('.', ',');
  say('chInfo', 'Âm thanh: ' + (c.custom ? 'file bạn đã tải lên' : 'mặc định (Happy Logo)') + ' · ' + s + ' giây');
}

// Giai ma file am thanh ngay tren dien thoai, doi sang mono 16 kHz 16 bit, cat toi da 10 giay,
// chuan hoa am luong roi gui len dong ho (dong ho khong phai giai ma MP3).
const CHIME_RATE = 16000, CHIME_MAX_SEC = 10;
$('chFile').onchange = async () => {
  const f = $('chFile').files[0];
  if (!f) return;
  say('msg7', 'Đang xử lý "' + f.name + '"...');
  try {
    const AC = window.AudioContext || window.webkitAudioContext;
    const ac = new AC();
    const audio = await ac.decodeAudioData(await f.arrayBuffer());
    ac.close();
    const secs = Math.min(audio.duration, CHIME_MAX_SEC);
    const trimmed = audio.duration > CHIME_MAX_SEC;
    const len = Math.max(1, Math.floor(secs * CHIME_RATE));
    const off = new OfflineAudioContext(1, len, CHIME_RATE);   // tu dong tron nhieu kenh ve mono
    const src = off.createBufferSource();
    src.buffer = audio; src.connect(off.destination); src.start();
    const data = (await off.startRendering()).getChannelData(0);

    let peak = 0.0001;
    for (let i = 0; i < data.length; i++) peak = Math.max(peak, Math.abs(data[i]));
    const gain = 0.89 / peak;
    const fadeIn = Math.floor(0.01 * CHIME_RATE), fadeOut = Math.min(data.length, Math.floor(0.12 * CHIME_RATE));
    const pcm = new Int16Array(data.length);
    for (let i = 0; i < data.length; i++) {
      let g = gain;
      if (i < fadeIn) g *= i / fadeIn;
      if (i >= data.length - fadeOut) g *= (data.length - i) / fadeOut;
      pcm[i] = Math.max(-32768, Math.min(32767, Math.round(data[i] * g * 32767)));
    }
    if (pcm.length < 1600) throw new Error('Âm thanh quá ngắn (cần ít nhất 0,1 giây)');

    const fd = new FormData();
    fd.append('snd', new Blob([pcm.buffer], { type: 'application/octet-stream' }), 'chime.raw');
    const t = await post('/api/chime/sound', fd);
    say('msg7', t + (trimmed ? ' File dài ' + audio.duration.toFixed(1) + ' giây nên đã cắt còn 10 giây.' : ''), 'ok');
    loadState();
  } catch (e) { say('msg7', 'Lỗi: ' + e.message, 'err'); }
  $('chFile').value = '';
};
$('chReset').onclick = async () => {
  try { say('msg7', await post('/api/chime/sound/reset'), 'ok'); loadState(); }
  catch (e) { say('msg7', 'Lỗi: ' + e.message, 'err'); }
};
async function sendChime(extra) {
  try {
    await post('/api/chime', new URLSearchParams(extra));
    say('msg7', 'Đã lưu.', 'ok');
  } catch (e) { say('msg7', 'Lỗi: ' + e.message, 'err'); }
}
$('chOn').onchange = () => { $('chVol').disabled = $('chQuiet').disabled = !$('chOn').checked; sendChime({ on: $('chOn').checked ? 1 : 0 }); };
$('chVol').onchange = () => sendChime({ vol: $('chVol').value });
$('chQuiet').onchange = () => sendChime({ quiet: $('chQuiet').checked ? 1 : 0 });
$('chTest').onclick = async () => {
  try { await post('/api/chime/test'); say('msg7', 'Đang phát thử...', 'ok'); }
  catch (e) { say('msg7', 'Lỗi: ' + e.message, 'err'); }
};

// ----- Duong lich / am lich -----
const dateBtns = [...document.querySelectorAll('button.dm')];
function showDate(mode) { dateBtns.forEach(b => b.classList.toggle('on', Number(b.dataset.m) === Number(mode))); }
dateBtns.forEach(b => b.onclick = async () => {
  try {
    await post('/api/datemode', new URLSearchParams({ mode: b.dataset.m }));
    showDate(b.dataset.m); say('msg6', 'Đã đổi sang: ' + b.textContent.toLowerCase() + '.', 'ok');
  } catch (e) { say('msg6', 'Lỗi: ' + e.message, 'err'); }
});

// ----- Khu vuc thoi tiet -----
function showLoc(l) {
  $('locAuto').checked = l.auto;
  $('locName').value = l.name; $('locLat').value = l.lat; $('locLon').value = l.lon;
  ['locName', 'locLat', 'locLon'].forEach(id => { $(id).disabled = l.auto; });
}
$('locAuto').onchange = async () => {
  const on = $('locAuto').checked;
  ['locName', 'locLat', 'locLon'].forEach(id => { $(id).disabled = on; });
  if (!on) { say('msg5', 'Nhập tên và toạ độ rồi bấm "Lưu khu vực".'); return; }
  try {
    say('msg5', await post('/api/location', new URLSearchParams({ auto: 1 })), 'ok');
    setTimeout(loadState, 4000);
  } catch (e) { say('msg5', 'Lỗi: ' + e.message, 'err'); }
};
$('locRefresh').onclick = async () => {
  try {
    say('msg5', await post('/api/location', new URLSearchParams({ auto: 1 })), 'ok');
    $('locAuto').checked = true; setTimeout(loadState, 4000);
  } catch (e) { say('msg5', 'Lỗi: ' + e.message, 'err'); }
};
$('locSave').onclick = async () => {
  try {
    await post('/api/location', new URLSearchParams({
      auto: $('locAuto').checked ? 1 : 0, name: $('locName').value, lat: $('locLat').value, lon: $('locLon').value }));
    say('msg5', 'Đã lưu khu vực.', 'ok');
  } catch (e) { say('msg5', 'Lỗi: ' + e.message, 'err'); }
};

// ----- WiFi -----
function showWifi(w) {
  const name = w.ssid ? '"' + w.ssid + '"' : '(chưa đặt)';
  let t;
  if (w.connected) t = 'Đang kết nối ' + name + ' — địa chỉ ' + w.ip;
  else if (w.ap) t = 'Đồng hồ chưa vào được WiFi ' + name + ' nên đang phát mạng cài đặt. Nhập WiFi nhà bạn bên dưới.';
  else t = 'Chưa kết nối ' + name;
  say('wst', t, w.connected ? 'ok' : '');
  if (!$('ssid').value) $('ssid').value = w.ssid || '';
}
$('wshow').onchange = () => { $('wpass').type = $('wshow').checked ? 'text' : 'password'; };
$('scan').onclick = async () => {
  $('scan').disabled = true; say('msg0', 'Đang quét (vài giây)...');
  try {
    const list = await (await fetch('/api/wifi/scan')).json();
    const box = $('nets'); box.textContent = '';
    const seen = new Set();
    list.sort((a, b) => b.rssi - a.rssi).forEach(n => {
      if (seen.has(n.ssid)) return; seen.add(n.ssid);
      const b = document.createElement('button'); b.className = 'net'; b.type = 'button';
      const name = document.createElement('span'); name.textContent = n.ssid;
      const info = document.createElement('small'); info.textContent = (n.lock ? 'có mật khẩu · ' : 'mở · ') + n.rssi + ' dBm';
      b.append(name, info);
      b.onclick = () => { $('ssid').value = n.ssid; $('wpass').focus(); };
      box.append(b);
    });
    say('msg0', seen.size ? 'Chạm một mạng để chọn.' : 'Không thấy mạng nào, thử quét lại.');
  } catch (e) { say('msg0', 'Lỗi quét: ' + e.message, 'err'); }
  $('scan').disabled = false;
};
$('wsave').onclick = async () => {
  const ssid = $('ssid').value.trim(), pass = $('wpass').value;
  if (!ssid) { say('msg0', 'Chưa nhập tên WiFi.', 'err'); return; }
  if (pass && pass.length < 8) { say('msg0', 'Mật khẩu WiFi cần ít nhất 8 ký tự (hoặc để trống nếu mạng mở).', 'err'); return; }
  try { say('msg0', await post('/api/wifi', new URLSearchParams({ ssid, pass })), 'ok'); }
  catch (e) { say('msg0', 'Lỗi: ' + e.message, 'err'); }
};

// ----- Man hinh nhac -----
const styleBtns = [...document.querySelectorAll('button.st')];
function showStyle(s) { styleBtns.forEach(b => b.classList.toggle('on', Number(b.dataset.s) === Number(s))); }
[['musicOn', 1, 'Đã bật màn hình nhạc.'], ['musicOff', 0, 'Đã về màn hình đồng hồ.']].forEach(([id, on, text]) => {
  $(id).onclick = async () => {
    try { await post('/api/music', new URLSearchParams({ on })); say('msg4', text, 'ok'); }
    catch (e) { say('msg4', 'Lỗi: ' + e.message, 'err'); }
  };
});
// Cham mot kieu: chon kieu do va bat man hinh nhac luon
styleBtns.forEach(b => b.onclick = async () => {
  try {
    await post('/api/music', new URLSearchParams({ on: 1, style: b.dataset.s }));
    showStyle(b.dataset.s); say('msg4', 'Đang hiện kiểu "' + b.textContent + '".', 'ok');
  } catch (e) { say('msg4', 'Lỗi: ' + e.message, 'err'); }
});

// ----- Mau chu: chon muc, roi keo tren vong tron mau (goc = sac mau, ban kinh = do dam) -----
const colors = {};                 // key -> "#rrggbb"
let current = 'time';              // muc dang chinh
let hsv = { h: 0, s: 0, v: 1 };
const swatches = [...document.querySelectorAll('.sw')];
const wheel = $('wheel'), wctx = wheel.getContext('2d');
const WR = wheel.width / 2;
let wheelImg = null, wheelV = -1;

function hsv2rgb(h, s, v) {
  const f = n => { const k = (n + h / 60) % 6; return v - v * s * Math.max(0, Math.min(k, 4 - k, 1)); };
  return [f(5), f(3), f(1)].map(x => Math.round(x * 255));
}
function rgb2hsv(r, g, b) {
  r /= 255; g /= 255; b /= 255;
  const mx = Math.max(r, g, b), d = mx - Math.min(r, g, b);
  let h = 0;
  if (d) {
    if (mx === r) h = ((g - b) / d) % 6; else if (mx === g) h = (b - r) / d + 2; else h = (r - g) / d + 4;
    h *= 60; if (h < 0) h += 360;
  }
  return { h, s: mx ? d / mx : 0, v: mx };
}
const toHex = rgb => '#' + rgb.map(x => x.toString(16).padStart(2, '0')).join('');
const fromHex = hex => [1, 3, 5].map(i => parseInt(hex.substr(i, 2), 16));

// Ve vong tron mau o do sang hien tai (ve lai khi do sang doi), roi ve dau chon
function drawWheel() {
  if (wheelV !== hsv.v) {
    wheelImg = wctx.createImageData(wheel.width, wheel.height);
    const d = wheelImg.data;
    for (let y = 0; y < wheel.height; y++) {
      for (let x = 0; x < wheel.width; x++) {
        const dx = x - WR + 0.5, dy = y - WR + 0.5, r = Math.hypot(dx, dy) / WR;
        if (r > 1) continue;
        let h = Math.atan2(dy, dx) * 180 / Math.PI; if (h < 0) h += 360;
        const c = hsv2rgb(h, r, hsv.v), i = (y * wheel.width + x) * 4;
        d[i] = c[0]; d[i + 1] = c[1]; d[i + 2] = c[2]; d[i + 3] = 255;
      }
    }
    wheelV = hsv.v;
  }
  wctx.putImageData(wheelImg, 0, 0);
  const a = hsv.h * Math.PI / 180, mx = WR + Math.cos(a) * hsv.s * WR, my = WR + Math.sin(a) * hsv.s * WR;
  wctx.lineWidth = 3; wctx.strokeStyle = '#000';
  wctx.beginPath(); wctx.arc(mx, my, 9, 0, 7); wctx.stroke();
  wctx.lineWidth = 2; wctx.strokeStyle = '#fff';
  wctx.beginPath(); wctx.arc(mx, my, 9, 0, 7); wctx.stroke();
}

function refreshSwatches() {
  swatches.forEach(b => {
    b.querySelector('i').style.background = colors[b.dataset.c] || '#000';
    b.classList.toggle('on', b.dataset.c === current);
  });
}

function selectItem(key) {
  current = key;
  const hex = colors[key] || '#ffffff';
  hsv = rgb2hsv(...fromHex(hex));
  $('val').value = hsv.v; $('hex').value = hex;
  refreshSwatches(); drawWheel();
}

// Cap nhat mau dang chinh tu hsv (xem truoc ngay tren trang)
function applyHsv() {
  const hex = toHex(hsv2rgb(hsv.h, hsv.s, hsv.v));
  colors[current] = hex; $('hex').value = hex;
  refreshSwatches(); drawWheel();
}

async function sendColor() {
  try {
    await post('/api/colors', new URLSearchParams({ [current]: colors[current] }));
    say('msg3', 'Đã đổi màu.', 'ok');
  } catch (e) { say('msg3', 'Lỗi: ' + e.message, 'err'); }
}

swatches.forEach(b => b.onclick = () => selectItem(b.dataset.c));

function pickAt(ev) {
  const rc = wheel.getBoundingClientRect(), k = wheel.width / rc.width;
  const dx = (ev.clientX - rc.left) * k - WR, dy = (ev.clientY - rc.top) * k - WR;
  let h = Math.atan2(dy, dx) * 180 / Math.PI; if (h < 0) h += 360;
  hsv.h = h; hsv.s = Math.min(1, Math.hypot(dx, dy) / WR);
  if (hsv.v < 0.05) { hsv.v = 1; $('val').value = 1; }   // dang den thi cho sang len de thay mau
  applyHsv();
}
let dragging = false;
wheel.onpointerdown = ev => { dragging = true; wheel.setPointerCapture(ev.pointerId); pickAt(ev); };
wheel.onpointermove = ev => { if (dragging) pickAt(ev); };
wheel.onpointerup = wheel.onpointercancel = () => { if (dragging) { dragging = false; sendColor(); } };

$('val').oninput = () => { hsv.v = parseFloat($('val').value); applyHsv(); };
$('val').onchange = sendColor;
$('hex').onchange = () => {
  let v = $('hex').value.trim(); if (v[0] !== '#') v = '#' + v;
  if (!/^#[0-9a-fA-F]{6}$/.test(v)) { say('msg3', 'Mã màu cần dạng #rrggbb', 'err'); return; }
  colors[current] = v.toLowerCase(); selectItem(current); sendColor();
};

$('colReset').onclick = async () => {
  try { await post('/api/colors/reset'); await loadState(); say('msg3', 'Đã về màu mặc định.', 'ok'); }
  catch (e) { say('msg3', 'Lỗi: ' + e.message, 'err'); }
};

$('save').onclick = async () => {
  try {
    await post('/api/sleep', new URLSearchParams({
      enabled: $('en').checked ? 1 : 0, start: $('st').value, end: $('et').value, idle: $('idle').value }));
    say('msg1', 'Đã lưu.', 'ok');
  } catch (e) { say('msg1', 'Lỗi: ' + e.message, 'err'); }
};
$('sleepNow').onclick = async () => {
  try { await post('/api/sleepnow'); say('msg1', 'Đồng hồ đã tắt màn hình và LED.', 'ok'); }
  catch (e) { say('msg1', 'Lỗi: ' + e.message, 'err'); }
};
$('wake').onclick = async () => {
  try { await post('/api/wake'); say('msg1', 'Đã đánh thức.', 'ok'); }
  catch (e) { say('msg1', 'Lỗi: ' + e.message, 'err'); }
};

// ----- Anh nen -----
let img = null;
const ctx = $('cv').getContext('2d', { willReadFrequently: true });

function draw() {
  ctx.fillStyle = '#000'; ctx.fillRect(0, 0, W, H);
  if (!img) return;
  // Phu kin khung 320x240, phong to them theo thanh truot, dich theo vi tri
  const s = Math.max(W / img.width, H / img.height) * parseFloat($('zoom').value);
  const w = img.width * s, h = img.height * s;
  ctx.imageSmoothingQuality = 'high';
  ctx.drawImage(img, (W - w) * parseFloat($('px').value), (H - h) * parseFloat($('py').value), w, h);
  // Lam toi, phia duoi toi hon de phan thoi tiet de doc
  const b = parseFloat($('br').value);
  const g = ctx.createLinearGradient(0, 0, 0, H);
  g.addColorStop(0, 'rgba(0,0,0,' + (1 - b) + ')');
  g.addColorStop(1, 'rgba(0,0,0,' + Math.min(1, 1 - b * 0.7) + ')');
  ctx.fillStyle = g; ctx.fillRect(0, 0, W, H);
}

$('file').onchange = () => {
  const f = $('file').files[0];
  if (!f) return;
  const i = new Image();
  i.onload = () => { img = i; $('up').disabled = false; draw(); say('msg2', 'Xem trước ở trên. Chỉnh xong thì bấm gửi.'); };
  i.onerror = () => say('msg2', 'Không mở được ảnh này.', 'err');
  i.src = URL.createObjectURL(f);
};
['zoom', 'px', 'py', 'br'].forEach(id => $(id).oninput = draw);

$('up').onclick = async () => {
  if (!img) return;
  $('up').disabled = true;
  say('msg2', 'Đang gửi...');
  try {
    const d = ctx.getImageData(0, 0, W, H).data;
    const out = new Uint16Array(W * H);   // RGB565, little-endian
    for (let i = 0, j = 0; i < out.length; i++, j += 4) {
      out[i] = ((d[j] >> 3) << 11) | ((d[j + 1] >> 2) << 5) | (d[j + 2] >> 3);
    }
    const fd = new FormData();
    fd.append('bg', new Blob([out.buffer], { type: 'application/octet-stream' }), 'bg.raw');
    const t = await post('/api/bg', fd);
    say('msg2', t, 'ok');
  } catch (e) { say('msg2', 'Lỗi: ' + e.message, 'err'); }
  $('up').disabled = false;
};

$('reset').onclick = async () => {
  try { say('msg2', await post('/api/bg/reset'), 'ok'); }
  catch (e) { say('msg2', 'Lỗi: ' + e.message, 'err'); }
};

// ----- Cap nhat firmware -----
function otaKeyQs() { return $('otaKey').value ? '?key=' + encodeURIComponent($('otaKey').value) : ''; }

async function otaStatus(timeoutMs) {
  const c = new AbortController(), t = setTimeout(() => c.abort(), timeoutMs || 3000);
  try { return await (await fetch('/api/ota/status', { signal: c.signal })).json(); }
  finally { clearTimeout(t); }
}

async function loadOta() {
  try {
    const s = await otaStatus();
    say('otaInfo', 'Phiên bản: ' + s.version + ' · phân vùng ' + s.partition);
    $('otaKeyRow').style.display = s.needKey ? '' : 'none';
    return s;
  } catch (e) { return null; }
}

// Doi dong ho khoi dong lai (uptime nho di) hoac bao loi; toi da 4 phut
async function otaWait(prevUp) {
  const end = Date.now() + 240000;
  while (Date.now() < end) {
    await new Promise(r => setTimeout(r, 2500));
    try {
      const s = await otaStatus();
      if (s.up < prevUp) { say('msg8', 'Xong! Đồng hồ đã khởi động lại, phiên bản ' + s.version, 'ok'); loadOta(); return; }
      if (!s.busy && s.msg && s.msg !== 'OK') { say('msg8', 'Cập nhật lỗi: ' + s.msg + ' (vẫn chạy bản cũ)', 'err'); return; }
    } catch (e) { /* dang ghi flash hoac dang khoi dong lai: server chua tra loi */ }
  }
  say('msg8', 'Chưa thấy đồng hồ trả lời, hãy kiểm tra màn hình rồi tải lại trang.', 'err');
}

$('otaUp').onclick = async () => {
  const f = $('otaFile').files[0];
  if (!f) { say('msg8', 'Hãy chọn file firmware.bin trước', 'err'); return; }
  if (!confirm('Cập nhật firmware từ "' + f.name + '"?')) return;
  const before = await loadOta();
  $('otaUp').disabled = true;
  const fd = new FormData();
  fd.append('fw', f, 'firmware.bin');
  const x = new XMLHttpRequest();
  x.open('POST', '/api/ota' + otaKeyQs());
  x.upload.onprogress = e => { if (e.lengthComputable) say('msg8', 'Đang gửi: ' + Math.round(100 * e.loaded / e.total) + '%'); };
  x.onload = () => {
    $('otaUp').disabled = false;
    if (x.status === 200) { say('msg8', 'Đã ghi xong, đang khởi động lại...'); otaWait(before ? before.up : 0); }
    else say('msg8', 'Lỗi: ' + (x.responseText || 'HTTP ' + x.status), 'err');
  };
  x.onerror = () => { $('otaUp').disabled = false; say('msg8', 'Mất kết nối khi gửi file', 'err'); };
  x.send(fd);
};

$('otaGo').onclick = async () => {
  const url = $('otaUrl').value.trim();
  if (!url) { say('msg8', 'Hãy nhập URL file firmware.bin', 'err'); return; }
  if (!confirm('Đồng hồ sẽ tải và cập nhật từ:\n' + url)) return;
  const before = await loadOta();
  try {
    const body = new URLSearchParams({ url, md5: $('otaMd5').value.trim(), key: $('otaKey').value });
    say('msg8', await post('/api/ota/url', body));
    otaWait(before ? before.up : 0);
  } catch (e) { say('msg8', 'Lỗi: ' + e.message, 'err'); }
};

selectItem(current);   // ve vong tron mau ngay, truoc khi doc xong cai dat
loadOta();
loadState();
</script>
</body>
</html>
)HTML";
