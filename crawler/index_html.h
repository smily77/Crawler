// Web page for FPV stream and web control (kept in a .h file so the
// Arduino preprocessor does not parse the JavaScript)
#pragma once

static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>RC Crawler 2d</title>
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
<style>
  * { box-sizing: border-box; -webkit-user-select: none; user-select: none; -webkit-touch-callout: none; -webkit-tap-highlight-color: transparent; }
  html, body { margin: 0; height: 100%; background: #14181f; color: #e6e9ef; font-family: Arial, sans-serif; overflow: hidden; }
  #wrap { display: flex; flex-direction: column; align-items: center; gap: 12px; padding: 10px; height: 100%; }
  #photo { width: 100%; max-width: 640px; border-radius: 8px; background: #000; min-height: 120px; }
  #ctrl { display: flex; align-items: center; justify-content: center; gap: 20px; }
  #pad { position: relative; width: min(62vw, 300px); aspect-ratio: 1; border-radius: 16px; background: #222936;
         border: 2px solid #3a4558; touch-action: none; }
  #pad::before, #pad::after { content: ""; position: absolute; background: #3a4558; }
  #pad::before { left: 50%; top: 8%; bottom: 8%; width: 1px; }
  #pad::after { top: 50%; left: 8%; right: 8%; height: 1px; }
  #knob { position: absolute; left: 50%; top: 50%; width: 22%; aspect-ratio: 1; transform: translate(-50%, -50%); border-radius: 50%;
          background: #4c8dff; box-shadow: 0 0 12px rgba(76,141,255,.6); z-index: 1; pointer-events: none; }
  #btns { display: flex; flex-direction: column; gap: 16px; }
  .btn { width: 92px; height: 92px; border-radius: 50%; border: 2px solid #3a4558; background: #222936; color: #e6e9ef;
         font-size: 16px; font-weight: bold; touch-action: none; cursor: pointer; }
  #light.on { background: #f5d547; color: #14181f; border-color: #f5d547; }
  #fire.on  { background: #ff4b3e; border-color: #ff4b3e; }
  #status { font-size: 12px; color: #ff8a80; height: 14px; text-align: center; }
  @media (orientation: landscape) and (max-height: 600px) {
    #wrap { flex-direction: row; justify-content: center; }
    #photo { width: auto; max-width: 55vw; max-height: 92vh; }
    #pad { width: min(70vh, 36vw, 300px); }
  }
</style>
</head>
<body>
<div id="wrap">
  <img id="photo" src="">
  <div>
    <div id="ctrl">
      <div id="pad"><div id="knob"></div></div>
      <div id="btns">
        <button id="light" class="btn">Light</button>
        <button id="fire" class="btn">Fire</button>
      </div>
    </div>
    <div id="status"></div>
  </div>
</div>
<script>
  const FORWARD_IS_NEGATIVE_Y = true;   // same convention as the old forward button (y = -speed)
  const STEER_FACTOR = 1 / 3;           // limit steering like the hardware joystick (0.33..0.66)
  const SEND_INTERVAL = 100;            // ms, must stay well below WEB_TIMEOUT on the crawler
  const pad = document.getElementById("pad"), knob = document.getElementById("knob");
  const fireBtn = document.getElementById("fire"), lightBtn = document.getElementById("light");
  const status = document.getElementById("status");
  let x = 0, y = 0, padDown = false, fire = false, light = false, busy = false, timer = null;

  document.getElementById("photo").src = location.protocol + "//" + location.hostname + ":81/stream";
  document.addEventListener("contextmenu", e => e.preventDefault());

  function send(url) {
    return fetch(url).then(() => { status.textContent = ""; })
                     .catch(() => { status.textContent = "no connection"; });
  }
  function sendXY(force) {
    if (busy && !force) return;          // skip instead of queueing old positions
    busy = true;
    send("/action?go=xy&x=" + x + "&y=" + y + "&f=" + (fire ? 1 : 0)).finally(() => { busy = false; });
  }
  function update() {                    // start/stop the periodic sender
    const active = padDown || fire;
    if (active && !timer) { sendXY(true); timer = setInterval(() => sendXY(false), SEND_INTERVAL); }
    if (!active && timer) { clearInterval(timer); timer = null; x = 0; y = 0; sendXY(true); }
  }
  function movePad(e) {
    const r = pad.getBoundingClientRect();
    let nx = ((e.clientX - r.left) / r.width) * 2 - 1;
    let ny = ((e.clientY - r.top) / r.height) * 2 - 1;
    nx = Math.max(-1, Math.min(1, nx)); ny = Math.max(-1, Math.min(1, ny));
    knob.style.left = ((nx + 1) * 50) + "%"; knob.style.top = ((ny + 1) * 50) + "%";
    x = Math.round(nx * 512 * STEER_FACTOR);
    y = Math.round((FORWARD_IS_NEGATIVE_Y ? ny : -ny) * 512);
  }
  function releasePad() {
    padDown = false; x = 0; y = 0;
    knob.style.left = "50%"; knob.style.top = "50%";
    update();
  }
  pad.addEventListener("pointerdown", e => { pad.setPointerCapture(e.pointerId); padDown = true; movePad(e); update(); });
  pad.addEventListener("pointermove", e => { if (padDown) movePad(e); });
  pad.addEventListener("pointerup", releasePad);
  pad.addEventListener("pointercancel", releasePad);

  function setFire(on) { fire = on; fireBtn.classList.toggle("on", on); update(); }
  fireBtn.addEventListener("pointerdown", e => { fireBtn.setPointerCapture(e.pointerId); setFire(true); });
  fireBtn.addEventListener("pointerup", () => setFire(false));
  fireBtn.addEventListener("pointercancel", () => setFire(false));

  lightBtn.addEventListener("click", () => {
    light = !light; lightBtn.classList.toggle("on", light);
    send("/action?go=light&v=" + (light ? 1 : 0));
  });

  document.addEventListener("visibilitychange", () => { if (document.hidden) { setFire(false); releasePad(); } });
</script>
</body>
</html>
)rawliteral";
