#pragma once

namespace homepoint::web {

inline constexpr const char kSetupPage[] PROGMEM = R"html(
<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Homepoint-M5 setup</title>
<style>
body{font-family:sans-serif;max-width:700px;margin:2rem auto;padding:0 1rem}
input,select,button{font-size:1rem;padding:.6rem;width:100%;box-sizing:border-box;margin:.3rem 0 1rem}
button{cursor:pointer}.muted{color:#666}
</style>
</head>
<body>
<h1>Homepoint-M5</h1>
<h2>Initial Wi-Fi setup</h2>
<p class="muted">This page is served from firmware and does not depend on LittleFS or MQTT.</p>
<form method="post" action="/api/setup">
<label>Wi-Fi network</label>
<select id="ssidList" onchange="document.getElementById('ssid').value=this.value">
<option>Scanning...</option>
</select>
<label>SSID</label><input id="ssid" name="ssid" required>
<label>Wi-Fi password</label><input name="wifiPassword" type="password">
<label>Hostname</label><input name="hostname" value="homepoint-m5">
<h3>Web administration</h3>
<label>Username</label><input name="webUsername" value="admin" required>
<label>Password</label><input name="webPassword" type="password" required>
<button type="submit">Save and reboot</button>
</form>
<script>
async function scan(){
 try{
  const d=await (await fetch('/api/wifi/scan')).json();
  const s=document.getElementById('ssidList'); s.innerHTML='<option value="">-- choose --</option>';
  d.networks.sort((a,b)=>b.rssi-a.rssi).forEach(n=>{
   const o=document.createElement('option');
   o.value=n.ssid; o.textContent=n.ssid+' ('+n.rssi+' dBm)'+(n.secure?' 🔒':'');
   s.appendChild(o);
  });
 }catch(e){document.getElementById('ssidList').innerHTML='<option>Scan failed</option>'}
}
scan();
</script>
</body>
</html>
)html";

inline constexpr const char kAdminPage[] PROGMEM = R"html(
<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Homepoint-M5</title>
<style>
body{font-family:sans-serif;max-width:1000px;margin:1.5rem auto;padding:0 1rem}
h1 small{font-size:.55em;font-weight:normal;color:#666}
textarea{width:100%;height:34rem;font-family:monospace;box-sizing:border-box}
button,input{font-size:1rem;padding:.5rem;margin:.3rem}
.row{display:flex;gap:.5rem;flex-wrap:wrap}.ok{color:green}.err{color:#b00}
code{background:#eee;padding:.15rem .3rem} ul{padding-left:1.3rem}.muted{color:#666}
</style>
</head>
<body>
<h1>Homepoint-M5 <small id="hostname"></small></h1>
<p id="status"></p>
<div class="row">
<button onclick="loadConfig()">Reload editor</button>
<button onclick="saveConfig()">Save config.json</button>
<button onclick="post('/api/reload')">Apply configuration</button>
<button onclick="post('/api/reboot')">Reboot</button>
</div>
<textarea id="config"></textarea>
<p id="message"></p>

<h2>Display</h2>
<label><input id="debugUi" type="checkbox" style="width:auto;margin-right:.5rem" onchange="setDebugUi(this.checked)">Debug display</label>
<p class="muted">Off = minimal icon-based end-user dashboard. On = text-heavy engineering/debug dashboard. This setting is stored in bootstrap EEPROM and does not depend on config.json.</p>

<h2>Files</h2>
<div class="row">
<input id="upload" type="file">
<button onclick="uploadFile()">Upload</button>
<button onclick="loadFiles()">Refresh list</button>
</div>
<ul id="files"></ul>

<h2>Firmware</h2>
<form method="post" action="/api/ota" enctype="multipart/form-data">
<input type="file" name="firmware" required>
<button type="submit">OTA update</button>
</form>

<h2>Recovery</h2>
<div class="row">
<button onclick="clearBootstrap()">Clear Wi-Fi/web bootstrap settings</button>
<button onclick="formatFs()">Format LittleFS</button>
</div>

<script>
const msg=(t,ok=true)=>{const e=document.getElementById('message');e.textContent=t;e.className=ok?'ok':'err'};
async function loadConfig(){
 const r=await fetch('/api/config'); document.getElementById('config').value=await r.text();
}
async function saveConfig(){
 const r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:document.getElementById('config').value});
 msg(await r.text(),r.ok);
}
async function post(url){
 const r=await fetch(url,{method:'POST'}); msg(await r.text(),r.ok);
}
async function loadFiles(){
 const d=await (await fetch('/api/files')).json(); const ul=document.getElementById('files');ul.innerHTML='';
 d.files.forEach(f=>{
   const li=document.createElement('li');
   const a=document.createElement('a');a.textContent=f.name+' ('+f.size+' B)';
   a.href='/api/file?path='+encodeURIComponent(f.name);a.target='_blank';li.appendChild(a);
   if(f.name!='/config.json' && f.name!='config.json'){
     const b=document.createElement('button');b.textContent='delete';
     b.onclick=async()=>{await fetch('/api/file?path='+encodeURIComponent(f.name),{method:'DELETE'});loadFiles()};
     li.appendChild(b);
   }
   ul.appendChild(li);
 });
}
async function uploadFile(){
 const f=document.getElementById('upload').files[0]; if(!f)return;
 const fd=new FormData();fd.append('file',f,f.name);
 const r=await fetch('/api/upload',{method:'POST',body:fd});msg(await r.text(),r.ok);loadFiles();
}
async function clearBootstrap(){
 if(confirm('Clear Wi-Fi/web bootstrap settings and reboot into setup AP?')) await post('/api/bootstrap/clear');
}
async function formatFs(){
 if(confirm('Erase all LittleFS files including config.json?')) await post('/api/fs/format');
}
async function identity(){
 const r=await fetch('/api/hostname');
 if(!r.ok)return;
 const h=(await r.text()).trim();
 document.getElementById('hostname').textContent=h ? '— '+h : '';
 if(h) document.title=h+' — Homepoint-M5';
}
async function loadDebugUi(){
 const r=await fetch('/api/ui/debug'); if(!r.ok)return;
 const d=await r.json(); document.getElementById('debugUi').checked=!!d.debug;
}
async function setDebugUi(enabled){
 const r=await fetch('/api/ui/debug?enabled='+(enabled?'1':'0'),{method:'POST'});
 msg(await r.text(),r.ok); if(!r.ok) loadDebugUi(); else status();
}
async function status(){
 const r=await fetch('/api/status'); document.getElementById('status').textContent=await r.text();
}
loadConfig();loadFiles();identity();loadDebugUi();status();
</script>
</body>
</html>
)html";

}  // namespace homepoint::web
