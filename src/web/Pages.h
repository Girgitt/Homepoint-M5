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
:root{font-family:system-ui,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;color:#1f2933;background:#f3f5f7}
*{box-sizing:border-box}body{max-width:1180px;margin:0 auto;padding:1rem}h1{margin:.2rem 0 0}h1 small{font-size:.5em;font-weight:500;color:#65717d}
h2{font-size:1.15rem;margin:0 0 .8rem}.status{margin:.5rem 0 1rem;color:#44515d}.cards{display:grid;grid-template-columns:minmax(340px,1fr) minmax(300px,.8fr);gap:1rem;margin-bottom:1rem}
.card{background:#fff;border:1px solid #d8dee4;border-radius:8px;padding:1rem;box-shadow:0 1px 2px #0000000d}.row{display:flex;gap:.5rem;align-items:center;flex-wrap:wrap}
button,input,select,textarea{font:inherit}button{padding:.48rem .72rem;border:1px solid #aeb8c2;border-radius:5px;background:#f8fafb;cursor:pointer}button:hover{background:#eef2f5}
button.primary{background:#2867a7;color:white;border-color:#2867a7}button.danger{color:#9b1c1c}.muted{color:#687581;font-size:.9rem}.ok{color:#18783c}.err{color:#b42318}
.screen-wrap{display:flex;justify-content:center;align-items:center;min-height:270px;background:#111;border-radius:6px;padding:.75rem;overflow:hidden}.screen-wrap img{max-width:100%;max-height:640px;width:auto;height:auto;object-fit:contain;image-rendering:auto;background:#000}
.file-toolbar{display:flex;gap:.5rem;align-items:center;flex-wrap:wrap;margin-bottom:.65rem}.file-toolbar input[type=search]{flex:1;min-width:180px;padding:.5rem;border:1px solid #b9c2ca;border-radius:5px}.path-input{min-width:220px;flex:1;padding:.48rem;border:1px solid #b9c2ca;border-radius:5px;font-family:ui-monospace,SFMono-Regular,Consolas,monospace}
.file-manager{display:grid;grid-template-columns:minmax(240px,34%) minmax(0,66%);height:640px;min-height:380px;border:1px solid #cfd6dc;border-radius:7px 7px 0 0;overflow:hidden;background:white}.file-list-pane{border-right:1px solid #cfd6dc;display:flex;flex-direction:column;min-width:0;min-height:0;overflow:hidden}.file-list{overflow-y:scroll;overflow-x:hidden;scrollbar-gutter:stable;flex:1;min-height:0;padding:.35rem}.file-entry{display:flex;justify-content:space-between;gap:.5rem;width:100%;padding:.46rem .5rem;border:0;border-radius:4px;background:transparent;text-align:left}.file-entry:hover{background:#eef4f8}.file-entry.selected{background:#dceaf6}.file-name{overflow:hidden;text-overflow:ellipsis;white-space:nowrap}.file-size{white-space:nowrap;color:#77828c;font-size:.82rem}.file-detail{display:flex;flex-direction:column;min-width:0;min-height:0;overflow:hidden}.file-head{padding:.65rem .8rem;border-bottom:1px solid #d8dee4;flex:none}.file-path{font-family:ui-monospace,SFMono-Regular,Consolas,monospace;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}.file-actions{margin-top:.5rem}.preview{flex:1;min-height:0;overflow:auto;padding:1rem;background:#f8fafb}.preview.editor-preview{overflow:hidden;padding:.75rem;display:flex;min-height:0}.image-preview{height:100%;min-height:250px;display:flex;align-items:center;justify-content:center;background:#1a1a1a;border-radius:5px}.image-preview img{max-width:100%;max-height:100%;object-fit:contain}.text-editor{width:100%;height:auto;min-height:0;flex:1;resize:none;overflow:auto;padding:.75rem;border:1px solid #bdc6ce;border-radius:5px;font-family:ui-monospace,SFMono-Regular,Consolas,monospace;font-size:.9rem;line-height:1.35;tab-size:2;background:white}.file-resize-handle{height:13px;border:1px solid #cfd6dc;border-top:0;border-radius:0 0 7px 7px;background:linear-gradient(#f8fafb,#edf1f4);cursor:ns-resize;display:flex;align-items:center;justify-content:center;touch-action:none;user-select:none}.file-resize-handle:before{content:'';width:54px;border-top:2px solid #aab4bd;border-bottom:2px solid #aab4bd;height:4px}.file-list,.text-editor{scrollbar-width:auto;scrollbar-color:#93a1ad #edf1f4}.file-list::-webkit-scrollbar,.text-editor::-webkit-scrollbar{width:12px;height:12px}.file-list::-webkit-scrollbar-track,.text-editor::-webkit-scrollbar-track{background:#edf1f4}.file-list::-webkit-scrollbar-thumb,.text-editor::-webkit-scrollbar-thumb{background:#93a1ad;border:2px solid #edf1f4;border-radius:8px}.file-resize-hint{margin:.35rem 0 0;color:#77828c;font-size:.82rem}.placeholder{display:flex;align-items:center;justify-content:center;height:100%;color:#75808a;text-align:center;padding:2rem}
.dashboard-card{margin-bottom:1rem}.dashboard-toolbar,.dashboard-source-toolbar,.dashboard-edit-toolbar{display:flex;gap:.45rem;align-items:center;flex-wrap:wrap}.dashboard-toolbar{margin-bottom:.65rem}.dashboard-source-toolbar{margin-bottom:.65rem;padding:.55rem .65rem;border:1px solid #d8dee4;border-radius:6px;background:#f7f9fa}.dashboard-source-toolbar label{display:flex;gap:.35rem;align-items:center;font-size:.86rem;color:#52606b}.dashboard-source-toolbar select,.dashboard-source-toolbar input{padding:.38rem .45rem;border:1px solid #b9c2ca;border-radius:5px;background:white}.dashboard-source-toolbar select{min-width:220px}.dashboard-source-toolbar input{min-width:190px}.dashboard-active-summary{font-size:.82rem;color:#52606b;margin-left:auto}.dashboard-banner{padding:.55rem .65rem;border-radius:5px;background:#eef4f8;color:#44515d;margin-bottom:.75rem;font-size:.9rem}.dashboard-banner.warn{background:#fff4d6;color:#76520a}.dashboard-banner.error{background:#fdecec;color:#8f1d1d}.dashboard-editor{display:grid;grid-template-columns:minmax(260px,.9fr) minmax(320px,1.05fr) minmax(270px,.95fr);gap:.8rem;align-items:stretch}.dashboard-pane{border:1px solid #cfd6dc;border-radius:7px;background:#fbfcfd;min-width:0;overflow:hidden}.dashboard-pane-head{display:flex;align-items:center;justify-content:space-between;gap:.5rem;padding:.55rem .65rem;border-bottom:1px solid #d8dee4;background:#f5f7f9}.dashboard-pane-head strong{font-size:.92rem}.dashboard-tree{padding:.35rem;max-height:430px;overflow:auto;min-height:260px}.dashboard-node{display:flex;align-items:center;gap:.45rem;width:100%;border:0;background:transparent;border-radius:4px;padding:.42rem .45rem;text-align:left;margin:1px 0}.dashboard-node:hover{background:#eef4f8}.dashboard-node.selected{background:#dceaf6;box-shadow:inset 3px 0 #2867a7}.dashboard-node.child{padding-left:1.65rem;font-size:.92rem}.dashboard-node-index{min-width:1.7rem;color:#71808d;font-variant-numeric:tabular-nums}.dashboard-node-name{flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}.dashboard-node-type{font-size:.78rem;color:#65717d;border:1px solid #d3d9de;border-radius:10px;padding:.05rem .4rem;background:white}.dashboard-edit-toolbar{padding:.5rem .55rem;border-top:1px solid #d8dee4;background:#f7f9fa}.dashboard-edit-toolbar button{padding:.32rem .5rem}.dashboard-preview-body{padding:.75rem;display:flex;flex-direction:column;align-items:center;gap:.55rem}.core2-screen{width:100%;max-width:430px;aspect-ratio:4/3;background:#000;border:7px solid #252a2e;border-radius:9px;box-shadow:0 2px 7px #0003;display:flex;flex-direction:column;overflow:hidden;color:white;user-select:none}.core2-status{height:8.333%;min-height:20px;display:grid;grid-template-columns:25px 1fr 25px;align-items:center;font-size:.68rem}.core2-status-side{text-align:center;color:#9fb0b4}.core2-status-center{text-align:center}.core2-grid{flex:1;display:grid;grid-template-columns:repeat(3,1fr);grid-template-rows:repeat(2,1fr);min-height:0}.core2-tile{position:relative;min-width:0;min-height:0;border:0;border-right:1px solid #4f666a;border-bottom:1px solid #4f666a;background:#000;color:white;padding:.2rem;display:flex;flex-direction:column;align-items:center;justify-content:center;overflow:hidden}.core2-tile:hover{background:#000}.core2-tile:nth-child(3n){border-right:0}.core2-tile:nth-child(n+4){border-bottom:0}.core2-tile.selected{outline:3px solid #5fa8e8;outline-offset:-3px}.core2-tile-icon{width:40%;height:42%;object-fit:contain;margin-bottom:.2rem}.core2-tile-fallback{font-size:1.6rem;line-height:1;margin-bottom:.15rem;color:#dbe6e8}.core2-tile-value{font-size:1rem;font-weight:600}.core2-tile-name{font-size:.7rem;max-width:95%;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;text-align:center}.core2-scene-more{position:absolute;right:.35rem;top:.25rem;font-weight:bold}.core2-live-age{position:absolute;left:.28rem;top:.22rem;font-size:.55rem;color:#8fa4aa}.core2-tile.live-active{color:#e0d360}.core2-footer{height:10%;min-height:24px;border-top:1px solid #4f666a;display:grid;grid-template-columns:auto 1fr auto;gap:.35rem;align-items:center;padding:0 .5rem;font-size:.68rem}.core2-footer-center{display:flex;justify-content:center;align-items:center;gap:.35rem;min-width:0}.core2-footer button{padding:.18rem .36rem;font-size:.68rem}.preview-controls{display:flex;align-items:center;justify-content:space-between;gap:.5rem;width:100%;flex-wrap:wrap}.preview-mode{display:flex;gap:.3rem}.preview-mode button{padding:.28rem .5rem}.preview-mode button.active{background:#2867a7;color:white;border-color:#2867a7}.live-status{font-size:.78rem}.live-status.ok{color:#18783c}.live-status.warn{color:#9a6700}.live-status.err{color:#b42318}.dashboard-inspector{padding:.65rem}.inspector-empty{color:#75808a;text-align:center;padding:2.5rem .8rem}.inspector-grid{display:grid;grid-template-columns:1fr;gap:.5rem}.inspector-field label{display:block;font-size:.78rem;color:#5b6873;margin-bottom:.15rem}.inspector-field input,.inspector-field select{width:100%;padding:.42rem .48rem;border:1px solid #b9c2ca;border-radius:5px;background:white}.inspector-check{display:flex;align-items:center;gap:.45rem;padding:.25rem 0}.inspector-check input{width:auto}.dashboard-dirty{font-size:.82rem;color:#a15c00}.dashboard-clean{font-size:.82rem;color:#18783c}
.footer-grid{display:grid;grid-template-columns:1fr 1fr;gap:1rem}.message{min-height:1.3rem;margin:.5rem 0}.hidden{display:none!important}
@media(max-width:1030px){.dashboard-editor{grid-template-columns:minmax(260px,.9fr) minmax(320px,1.1fr)}.dashboard-inspector-pane{grid-column:1/-1}}
@media(max-width:780px){body{padding:.65rem}.cards,.footer-grid{grid-template-columns:1fr}.dashboard-editor{grid-template-columns:1fr}.dashboard-inspector-pane{grid-column:auto}.dashboard-tree{max-height:320px}.file-manager{grid-template-columns:1fr;height:auto;min-height:0;border-radius:7px}.file-list-pane{border-right:0;border-bottom:1px solid #cfd6dc}.file-list{height:260px;flex:none}.file-detail{height:560px}.file-resize-handle,.file-resize-hint{display:none}.screen-wrap{min-height:220px}}
</style>
</head>
<body>
<h1>Homepoint-M5 <small id="hostname"></small></h1>
<p id="status" class="status"></p>
<p id="message" class="message"></p>

<div class="cards">
<section class="card">
<h2>Device display</h2>
<div class="screen-wrap"><img id="screen" alt="Current Homepoint display"></div>
<div class="row" style="margin-top:.7rem">
<button class="primary" onclick="refreshScreen()">Refresh screenshot</button>
<button id="screenDownload" onclick="downloadScreen()" disabled>Download</button>
<label>Auto <select id="screenAuto" onchange="setScreenAuto(this.value)"><option value="0">Off</option><option value="2000">2 s</option><option value="5000">5 s</option><option value="10000">10 s</option></select></label>
</div>
<p id="screenMeta" class="muted">Screenshot is captured from the actual LCD on request.</p>
</section>
<section class="card">
<h2>Device controls</h2>
<div class="row">
<button onclick="selectFile('/config.json',true)">Open config.json</button>
<button onclick="post('/api/reload')">Apply configuration</button>
<button onclick="post('/api/reboot')">Reboot</button>
</div>
<p><label><input id="debugUi" type="checkbox" style="width:auto;margin-right:.5rem" onchange="setDebugUi(this.checked)">Debug display</label></p>
<p class="muted">Debug mode is stored in bootstrap EEPROM. Application configuration, including hostname, lives in config.json.</p>
</section>
</div>

<section class="card dashboard-card" id="dashboardCard">
<h2>Dashboard configuration</h2>
<div class="dashboard-source-toolbar">
<label>Editing <select id="layoutSelector" onchange="switchDashboardLayout(this.value)"><option>Loading…</option></select></label>
<label id="layoutNameWrap" class="hidden">Name <input id="layoutName" maxlength="96" oninput="dashboardLayoutNameChanged(this.value)"></label>
<button id="layoutNew" onclick="createDashboardLayout()">+ New layout</button>
<button id="layoutSetActive" onclick="setSelectedLayoutActive()" disabled>Set as active</button>
<button id="layoutUseInline" onclick="useSelectedLayoutInline()" disabled>Use inline copy</button>
<button id="layoutUpgrade" onclick="upgradeDashboardSchema()" disabled>Upgrade schema</button>
<span id="activeLayoutSummary" class="dashboard-active-summary">Active source: loading…</span>
</div>
<div class="dashboard-toolbar">
<button onclick="loadDashboard()">Refresh</button>
<button id="dashboardRevert" onclick="revertDashboard()" disabled>Revert</button>
<button id="dashboardValidate" onclick="validateDashboard()" disabled>Validate</button>
<button id="dashboardSave" class="primary" onclick="saveDashboard()" disabled>Save</button>
<span id="dashboardDirtyState" class="dashboard-clean">Not loaded</span>
</div>
<div id="dashboardBanner" class="dashboard-banner">Loading dashboard configuration…</div>
<div class="dashboard-editor">
<div class="dashboard-pane">
<div class="dashboard-pane-head"><strong>Dashboard structure</strong><span id="dashboardCount" class="muted"></span></div>
<div id="dashboardTree" class="dashboard-tree"><div class="placeholder">Loading…</div></div>
<div class="dashboard-edit-toolbar">
<button id="dashboardAddTileSwitch" onclick="addDashboardTile('switch')">+ Switch</button>
<button id="dashboardAddTileSensor" onclick="addDashboardTile('sensor')">+ Sensor</button>
<button id="dashboardAddTileScene" onclick="addDashboardTile('scene')">+ Scene</button>
<button id="dashboardAddSwitchItem" onclick="addSceneItem('switch')" disabled>+ Scene switch</button>
<button id="dashboardAddSensorItem" onclick="addSceneItem('sensor')" disabled>+ Scene sensor</button>
</div>
<div class="dashboard-edit-toolbar">
<button id="dashboardMoveUp" onclick="moveDashboardSelection(-1)" disabled>↑</button>
<button id="dashboardMoveDown" onclick="moveDashboardSelection(1)" disabled>↓</button>
<button id="dashboardDelete" class="danger" onclick="deleteDashboardSelection()" disabled>Delete</button>
</div>
</div>
<div class="dashboard-pane">
<div class="dashboard-pane-head"><strong>Core2 preview</strong><span id="dashboardPreviewModeLabel" class="muted">configuration</span></div>
<div class="dashboard-preview-body">
<div id="dashboardScreen" class="core2-screen">
<div class="core2-status"><span id="dashboardPreviewWifi" class="core2-status-side">WiFi</span><span id="dashboardPreviewTitle" class="core2-status-center">12:34</span><span id="dashboardPreviewMqtt" class="core2-status-side">MQTT</span></div>
<div id="dashboardPreviewGrid" class="core2-grid"></div>
<div id="dashboardPreviewFooter" class="core2-footer hidden">
<button id="dashboardPreviewPrev" onclick="changeDashboardPreviewPage(-1)">◀</button>
<div class="core2-footer-center"><button id="dashboardPreviewBack" class="hidden" onclick="closeDashboardPreviewScene()">Back</button><span id="dashboardPreviewPage"></span></div>
<button id="dashboardPreviewNext" onclick="changeDashboardPreviewPage(1)">▶</button>
</div>
</div>
<div class="preview-controls">
<div class="preview-mode"><button id="dashboardPreviewConfigure" class="active" onclick="setDashboardPreviewMode('configure')">Configure</button><button id="dashboardPreviewLive" onclick="setDashboardPreviewMode('live')">Live state</button></div>
<span id="dashboardPreviewHint" class="muted">Click selects. Double-click a scene to open it.</span>
<span id="dashboardLiveStatus" class="live-status"></span>
</div>
</div>
</div>
<div class="dashboard-pane dashboard-inspector-pane">
<div class="dashboard-pane-head"><strong>Inspector</strong><span id="dashboardSelectionLabel" class="muted"></span></div>
<div id="dashboardInspector" class="dashboard-inspector"><div class="inspector-empty">Select a tile or scene item.</div></div>
</div>
</div>
</section>

<section class="card" id="filesCard">
<h2>Files</h2>
<div class="file-toolbar">
<input id="fileSearch" type="search" placeholder="Filter files…" oninput="renderFileList()">
<button onclick="loadFiles()">Refresh</button>
<button onclick="createTextFile()">New text/JSON</button>
<input id="upload" type="file" onchange="suggestUploadPath()">
<input id="uploadPath" class="path-input" type="text" placeholder="/destination/path">
<button onclick="uploadFile()">Upload</button>
</div>
<div id="fileManager" class="file-manager">
<div class="file-list-pane"><div id="fileList" class="file-list"></div></div>
<div class="file-detail">
<div class="file-head">
<div id="selectedPath" class="file-path">No file selected</div>
<div id="selectedMeta" class="muted"></div>
<div id="fileActions" class="row file-actions hidden">
<button id="saveFileButton" class="primary hidden" onclick="saveSelectedText()">Save</button>
<button onclick="downloadSelected()">Download</button>
<button id="deleteFileButton" class="danger" onclick="deleteSelected()">Delete</button>
</div>
</div>
<div id="preview" class="preview"><div class="placeholder">Select a file from the list. The list and preview scroll independently.</div></div>
</div>
</div>
<div id="fileResizeHandle" class="file-resize-handle" title="Drag vertically to resize the file list and editor"></div>
<p class="file-resize-hint">Drag the bar below the file manager to make the list and editor taller or shorter.</p>
</section>

<div class="footer-grid" style="margin-top:1rem">
<section class="card">
<h2>Firmware</h2>
<form method="post" action="/api/ota" enctype="multipart/form-data">
<input type="file" name="firmware" required>
<button type="submit">OTA firmware update</button>
</form>
<p class="muted">Upload the PlatformIO firmware.bin application image. LittleFS is preserved.</p>
</section>
<section class="card">
<h2>Recovery</h2>
<div class="row">
<button onclick="clearBootstrap()">Clear Wi-Fi/web bootstrap</button>
<button class="danger" onclick="formatFs()">Format LittleFS</button>
</div>
</section>
</div>

<script>
let files=[];
let selected=null;
let screenUrl=null;
let screenBlob=null;
let screenTimer=null;
let screenAutoMs=0;
let screenRefreshing=false;
let screenRefreshQueued=false;
let editorDirty=false;
let selectionSerial=0;
let dashboardOriginal=null;
let dashboardDraft=null;
let dashboardSelection=null;
let dashboardPreviewPage=0;
let dashboardPreviewScene=null;
let dashboardPreviewMode='configure';
let dashboardRuntimeState=null;
let dashboardLiveTimer=null;
let dashboardLiveLoading=false;
let dashboardPreviewClickTimer=null;
let dashboardDirty=false;
let dashboardBusy=false;
let layoutCatalog=null;
let dashboardEditingFile=null;
let dashboardLayoutName='';
let dashboardOriginalLayoutName='';
const editableExt=['json','txt','md','css','js','html','htm','csv','log'];
const imageExt=['jpg','jpeg','png','gif','bmp','svg'];
const byId=id=>document.getElementById(id);
const ext=path=>{const n=(path||'').toLowerCase();const i=n.lastIndexOf('.');return i<0?'':n.slice(i+1)};
const norm=path=>path&&path.startsWith('/')?path:'/'+(path||'');
const formatBytes=n=>{if(n<1024)return n+' B';if(n<1024*1024)return (n/1024).toFixed(1)+' KiB';return (n/1024/1024).toFixed(1)+' MiB'};
const msg=(t,ok=true)=>{const e=byId('message');e.textContent=t;e.className='message '+(ok?'ok':'err')};
async function post(url){const r=await fetch(url,{method:'POST'});msg(await r.text(),r.ok);return r}
async function identity(){const r=await fetch('/api/hostname');if(!r.ok)return;const h=(await r.text()).trim();byId('hostname').textContent=h?'— '+h:'';if(h)document.title=h+' — Homepoint-M5'}
async function status(){const r=await fetch('/api/status');if(r.ok)byId('status').textContent=await r.text()}
async function loadDebugUi(){const r=await fetch('/api/ui/debug');if(!r.ok)return;const d=await r.json();byId('debugUi').checked=!!d.debug}
async function setDebugUi(enabled){const r=await fetch('/api/ui/debug?enabled='+(enabled?'1':'0'),{method:'POST'});msg(await r.text(),r.ok);if(!r.ok)loadDebugUi();else{status();refreshScreen()}}

const sleep=ms=>new Promise(resolve=>setTimeout(resolve,ms));
function clearScreenTimer(){if(screenTimer){clearTimeout(screenTimer);screenTimer=null}}
function scheduleScreenRefresh(delayMs=screenAutoMs){
 clearScreenTimer();
 if(screenAutoMs<=0)return;
 screenTimer=setTimeout(()=>{screenTimer=null;if(document.hidden){scheduleScreenRefresh(screenAutoMs);return}refreshScreen()},Math.max(100,delayMs));
}
async function refreshScreen(){
 if(screenRefreshing){screenRefreshQueued=true;return}clearScreenTimer();screenRefreshing=true;const started=performance.now();const meta=byId('screenMeta');meta.textContent='Capturing…';
 try{
  const start=await fetch('/api/screen/capture',{method:'POST',cache:'no-store'});if(!start.ok)throw new Error(await start.text());const id=(await start.json()).id;if(!id)throw new Error('Capture request failed');
  let r=null;const deadline=performance.now()+8000;while(performance.now()<deadline){await sleep(75);r=await fetch('/api/screen.bmp?id='+encodeURIComponent(id)+'&t='+Date.now(),{cache:'no-store'});if(r.status===202)continue;break}
  if(!r||r.status===202)throw new Error('Capture timed out');if(!r.ok)throw new Error(await r.text());
  const blob=await r.blob();if(screenUrl)URL.revokeObjectURL(screenUrl);screenBlob=blob;screenUrl=URL.createObjectURL(blob);const image=byId('screen');const capturedAt=new Date().toLocaleTimeString();const elapsed=Math.round(performance.now()-started);image.onload=()=>{meta.textContent='LCD '+image.naturalWidth+'×'+image.naturalHeight+' · '+formatBytes(blob.size)+' · '+capturedAt+' · '+elapsed+' ms'};image.src=screenUrl;
  byId('screenDownload').disabled=false;
 }catch(e){meta.textContent='Screenshot failed: '+e.message}finally{const elapsed=performance.now()-started;screenRefreshing=false;if(screenRefreshQueued){screenRefreshQueued=false;setTimeout(refreshScreen,0)}else if(screenAutoMs>0)scheduleScreenRefresh(Math.max(100,screenAutoMs-elapsed))}
}
function setScreenAuto(ms){screenAutoMs=Math.max(0,Number(ms)||0);clearScreenTimer();if(screenAutoMs>0){if(document.hidden)scheduleScreenRefresh(screenAutoMs);else refreshScreen()}}
function downloadScreen(){if(!screenBlob||!screenUrl)return;const a=document.createElement('a');a.href=screenUrl;a.download='homepoint-screen.bmp';a.click()}

async function loadFiles(){
 try{const r=await fetch('/api/files');if(!r.ok)throw new Error(await r.text());const d=await r.json();files=(d.files||[]).filter(f=>!f.directory).map(f=>({name:norm(f.name),size:f.size||0,editable:!!f.editable,deletable:!!f.deletable,uploadable:!!f.uploadable})).sort((a,b)=>a.name.localeCompare(b.name));if(selected){const refreshed=files.find(f=>f.name===selected.name);if(!refreshed){clearSelection();return}selected=refreshed}renderFileList()}
 catch(e){msg('Could not load files: '+e.message,false)}
}
function renderFileList(){
 const q=byId('fileSearch').value.trim().toLowerCase();const root=byId('fileList');const scrollTop=root.scrollTop;root.innerHTML='';
 files.filter(f=>!q||f.name.toLowerCase().includes(q)).forEach(f=>{const b=document.createElement('button');b.className='file-entry'+(selected&&selected.name===f.name?' selected':'');b.dataset.path=f.name;b.onclick=()=>selectFile(f.name);const n=document.createElement('span');n.className='file-name';n.textContent=f.name;const z=document.createElement('span');z.className='file-size';z.textContent=formatBytes(f.size);b.append(n,z);root.appendChild(b)});root.scrollTop=scrollTop;
}
async function selectFile(path,reveal=false){
 path=norm(path);if(editorDirty&&!confirm('Discard unsaved changes to '+(selected?selected.name:'the current file')+'?'))return;
 const serial=++selectionSerial;let f=files.find(x=>x.name===path);if(!f){await loadFiles();if(serial!==selectionSerial)return;f=files.find(x=>x.name===path)}if(!f){msg('File not found: '+path,false);return}
 editorDirty=false;selected=f;renderFileList();byId('selectedPath').textContent=f.name;byId('selectedMeta').textContent=formatBytes(f.size)+' · '+(ext(f.name)||'file')+(f.editable?'':' · read-only');byId('fileActions').classList.remove('hidden');byId('deleteFileButton').disabled=!f.deletable;byId('uploadPath').value=f.uploadable?f.name:'';
 if(reveal){document.getElementById('filesCard').scrollIntoView({behavior:'smooth',block:'start'});const entry=byId('fileList').querySelector('.file-entry.selected');if(entry)entry.scrollIntoView({block:'nearest'})}
 const p=byId('preview');p.className='preview';p.innerHTML='';const e=ext(f.name);const textLike=editableExt.includes(e);const viewableText=textLike&&f.size<=128*1024;const editable=f.editable&&viewableText;byId('saveFileButton').classList.toggle('hidden',!editable);
 if(imageExt.includes(e)){const box=document.createElement('div');box.className='image-preview';const img=document.createElement('img');img.alt=f.name;img.src='/api/file?path='+encodeURIComponent(f.name)+'&t='+Date.now();box.appendChild(img);p.appendChild(box);return}
 if(viewableText){p.classList.add('editor-preview');const r=await fetch('/api/file?path='+encodeURIComponent(f.name),{cache:'no-store'});if(serial!==selectionSerial||!selected||selected.name!==f.name)return;if(!r.ok){p.innerHTML='<div class="placeholder">Could not load file.</div>';return}const content=await r.text();if(serial!==selectionSerial||!selected||selected.name!==f.name)return;const ta=document.createElement('textarea');ta.id='fileEditor';ta.className='text-editor';ta.spellcheck=false;ta.value=content;ta.readOnly=!editable;if(editable)ta.addEventListener('input',()=>{editorDirty=true});p.appendChild(ta);return}
 if(textLike&&f.size>128*1024){p.innerHTML='<div class="placeholder">This text file is larger than the 128 KiB browser-view/edit limit.<br>Use Download to save the original file.</div>';return}
 p.innerHTML='<div class="placeholder">No inline preview for this file type.<br>Use Download to save the original file.</div>';
}
function clearSelection(){selectionSerial++;editorDirty=false;selected=null;renderFileList();byId('selectedPath').textContent='No file selected';byId('selectedMeta').textContent='';byId('fileActions').classList.add('hidden');byId('preview').className='preview';byId('preview').innerHTML='<div class="placeholder">Select a file from the list. The list and preview scroll independently.</div>'}
async function saveSelectedText(){
 if(!selected)return;const ta=byId('fileEditor');if(!ta)return;const path=selected.name;if(path==='/config.json'&&dashboardDirty&&!confirm('Saving raw config.json will make the current dashboard draft stale and discard it after save. Continue?'))return;
 const r=await fetch('/api/file/text?path='+encodeURIComponent(path),{method:'POST',headers:{'Content-Type':'text/plain; charset=utf-8'},body:ta.value});const t=await r.text();if(r.ok){editorDirty=false;await loadFiles();selected=files.find(f=>f.name===path)||selected;byId('selectedMeta').textContent=formatBytes(new Blob([ta.value]).size)+' · '+ext(path);if(path==='/config.json')await loadDashboard(true);msg(path==='/config.json'&&!t.includes('rebooting')?t+'; use Apply configuration to activate runtime changes':t,true)}else msg(t,false)
}
function downloadSelected(){if(!selected)return;const a=document.createElement('a');a.href='/api/file?download=1&path='+encodeURIComponent(selected.name);a.download=selected.name.split('/').pop();a.click()}
async function deleteSelected(){if(!selected||selected.name==='/config.json')return;if(!confirm('Delete '+selected.name+'?'))return;const r=await fetch('/api/file?path='+encodeURIComponent(selected.name),{method:'DELETE'});msg(await r.text(),r.ok);if(r.ok){clearSelection();loadFiles()}}
async function createTextFile(){
 if(editorDirty&&!confirm('Discard unsaved changes before opening a new file?'))return;let path=prompt('New file path (for example /devices.json):','/new.json');if(!path)return;path=norm(path.trim());if(/^\/layout_[A-Za-z0-9_-]+\.json$/.test(path)){msg('Use + New layout in Dashboard configuration for managed layout files.',false);return}if(!editableExt.includes(ext(path))){msg('New editor files must use a text/JSON extension.',false);return}if(files.some(f=>f.name===path)){msg('File already exists; select it to edit or replace it explicitly.',false);return}
 const initial=ext(path)==='json'?'{\n  \n}\n':'';const r=await fetch('/api/file/text?path='+encodeURIComponent(path),{method:'POST',headers:{'Content-Type':'text/plain; charset=utf-8'},body:initial});msg(await r.text(),r.ok);if(r.ok){await loadFiles();selectFile(path)}
}
function suggestUploadPath(){const f=byId('upload').files[0];if(!f)return;byId('uploadPath').value=selected&&selected.uploadable?selected.name:norm(f.name)}
async function uploadFile(){const f=byId('upload').files[0];if(!f)return;const path=norm((byId('uploadPath').value||f.name).trim());const existing=files.find(x=>x.name===path);if(existing&&!existing.uploadable){msg('That managed file cannot be replaced through Upload.',false);return}if(path==='/config.json'){msg('Use the JSON editor for config.json so it is validated before activation.',false);return}const replacingDirty=!!(existing&&selected&&selected.name===path&&editorDirty);if(existing&&!confirm(replacingDirty?'Replace '+path+' and discard its unsaved editor changes?':'Replace existing file '+path+'?'))return;const fd=new FormData();fd.append('file',f,f.name);const r=await fetch('/api/upload?path='+encodeURIComponent(path),{method:'POST',body:fd});msg(await r.text(),r.ok);if(r.ok){if(replacingDirty)editorDirty=false;await loadFiles();if(files.some(x=>x.name===path))selectFile(path)}}
const deepClone=value=>JSON.parse(JSON.stringify(value));
const dashboardTiles=()=>dashboardDraft&&dashboardDraft.dashboard&&Array.isArray(dashboardDraft.dashboard.tiles)?dashboardDraft.dashboard.tiles:[];
const dashboardCaps=()=>dashboardDraft&&dashboardDraft.capabilities?dashboardDraft.capabilities:{};
function dashboardApiError(data,fallback){return data&&data.error?data.error:fallback}
async function dashboardJsonResponse(r){let data=null;try{data=await r.json()}catch(e){}if(!r.ok)throw new Error(dashboardApiError(data,'Dashboard request failed ('+r.status+')'));return data}
function dashboardSetBanner(text,kind=''){const b=byId('dashboardBanner');b.textContent=text;b.className='dashboard-banner'+(kind?' '+kind:'')}
function dashboardSourceSummary(){if(!dashboardDraft)return 'Dashboard not loaded';const src=dashboardDraft.source||{};let text=dashboardEditingFile?'Editing: '+(dashboardLayoutName||dashboardEditingFile)+' ('+dashboardEditingFile+')':'Editing: Inline — config.json';if(src.legacy)text+=' · legacy configuration';if(src.layoutRecovered)text+=' · recovered layout';const warnings=Array.isArray(dashboardDraft.warnings)?dashboardDraft.warnings:[];if(warnings.length)text+=' · '+warnings.join(' · ');return text}
function layoutActiveFile(){return layoutCatalog&&layoutCatalog.active&&layoutCatalog.active.type==='file'?layoutCatalog.active.file:null}
function editingLayoutIsActive(){return dashboardEditingFile?dashboardEditingFile===layoutActiveFile():!!(layoutCatalog&&layoutCatalog.active&&layoutCatalog.active.type==='inline')}
function dashboardLiveAvailable(){return !!dashboardDraft&&!dashboardBusy&&!dashboardDirty&&editingLayoutIsActive()}
function runtimeFingerprintMatchesEditor(state){const expected=dashboardDraft&&dashboardDraft.dashboardFingerprint;return typeof expected==='string'&&expected.length>0&&state&&typeof state.dashboardFingerprint==='string'&&state.dashboardFingerprint===expected}
function runtimeIdentityText(state){const expected=dashboardDraft&&dashboardDraft.dashboardFingerprint||'<missing>';const actual=state&&state.dashboardFingerprint||'<missing>';const editorSource=dashboardEditingFile||'inline';const runtimeSource=state&&state.source?(state.source.type==='file'?(state.source.file||'<unnamed-file>'):(state.source.type||'<unknown>')):'<missing>';return 'expected '+expected+' from '+editorSource+'; runtime '+actual+' from '+runtimeSource}
function clearDashboardLiveTimer(){if(dashboardLiveTimer){clearTimeout(dashboardLiveTimer);dashboardLiveTimer=null}}
function scheduleDashboardLivePoll(delay=1000){clearDashboardLiveTimer();if(dashboardPreviewMode!=='live')return;dashboardLiveTimer=setTimeout(()=>{dashboardLiveTimer=null;if(document.hidden){scheduleDashboardLivePoll(1000);return}loadDashboardRuntimeState()},Math.max(250,delay))}
function dashboardAgeLabel(ageMs){if(ageMs===undefined||ageMs===null)return'?';const s=Math.floor(Number(ageMs)/1000);if(s<2)return'now';if(s<60)return s+'s';const m=Math.floor(s/60);if(m<60)return m+'m';return Math.floor(m/60)+'h'}
function runtimeTileAt(index){return dashboardRuntimeState&&Array.isArray(dashboardRuntimeState.tiles)?dashboardRuntimeState.tiles.find(t=>Number(t.index)===Number(index)):null}
function runtimeItemAt(tileIndex,itemIndex){const t=runtimeTileAt(tileIndex);return t&&Array.isArray(t.items)?t.items.find(i=>Number(i.index)===Number(itemIndex)):null}
function updateDashboardPreviewModeControls(){const live=dashboardPreviewMode==='live';const liveButton=byId('dashboardPreviewLive');const configureButton=byId('dashboardPreviewConfigure');if(!liveButton||!configureButton)return;liveButton.disabled=!live&&!dashboardLiveAvailable();liveButton.classList.toggle('active',live);configureButton.classList.toggle('active',!live);byId('dashboardPreviewModeLabel').textContent=live?'read-only live state':'configuration';const hint=byId('dashboardPreviewHint');hint.textContent=live?'Read-only: click selects, double-click a scene opens it. No MQTT writes are sent.':'Click selects. Double-click a scene to open it.';const status=byId('dashboardLiveStatus');if(status&&!live){status.textContent='';status.className='live-status'}if(byId('layoutName'))byId('layoutName').disabled=dashboardBusy||live}
async function setDashboardPreviewMode(mode){if(mode!=='live'){dashboardPreviewMode='configure';dashboardRuntimeState=null;clearDashboardLiveTimer();renderDashboard();return}if(!dashboardLiveAvailable()){msg('Live state is available only for the saved active dashboard.',false);return}dashboardPreviewMode='live';dashboardRuntimeState=null;renderDashboard();await loadDashboardRuntimeState()}
async function loadDashboardRuntimeState(){if(dashboardPreviewMode!=='live'||dashboardLiveLoading)return;const firstLoad=dashboardRuntimeState===null;dashboardLiveLoading=true;const statusEl=byId('dashboardLiveStatus');if(statusEl){statusEl.textContent='Loading live state…';statusEl.className='live-status'}try{const r=await fetch('/api/dashboard/state',{cache:'no-store'});const d=await dashboardJsonResponse(r);if(dashboardPreviewMode!=='live')return;if(!runtimeFingerprintMatchesEditor(d)){dashboardPreviewMode='configure';dashboardRuntimeState=null;clearDashboardLiveTimer();renderDashboard();dashboardSetBanner('The active runtime dashboard differs from the editor ('+runtimeIdentityText(d)+'). Reload the editor before using Live state.','warn');return}dashboardRuntimeState=d;renderDashboardPreview();if(statusEl){const wi=d.wifi||{},mq=d.mqtt||{};statusEl.textContent='WiFi '+(wi.status||'UNKNOWN')+' · MQTT '+(mq.status||'UNKNOWN');statusEl.className='live-status '+(wi.connected&&mq.connected?'ok':'warn')}}catch(e){if(statusEl){const warming=e.message==='Runtime state refresh pending';statusEl.textContent=warming?'Refreshing live state…':'Live state unavailable: '+e.message;statusEl.className=warming?'live-status':'live-status err'}}finally{dashboardLiveLoading=false;scheduleDashboardLivePoll(firstLoad?300:1000)}}
function updateDashboardDirty(){if(!dashboardDraft||!dashboardOriginal){dashboardDirty=false}else{dashboardDirty=JSON.stringify(dashboardDraft.dashboard)!==JSON.stringify(dashboardOriginal.dashboard)||(dashboardEditingFile&&dashboardLayoutName!==dashboardOriginalLayoutName)}if(dashboardDirty&&dashboardPreviewMode==='live'){dashboardPreviewMode='configure';dashboardRuntimeState=null;clearDashboardLiveTimer()}const live=dashboardPreviewMode==='live';const e=byId('dashboardDirtyState');e.textContent=dashboardDirty?'Unsaved changes':'Saved';e.className=dashboardDirty?'dashboard-dirty':'dashboard-clean';byId('dashboardRevert').disabled=!dashboardDirty||dashboardBusy||live;byId('dashboardValidate').disabled=!dashboardDraft||dashboardBusy||live;byId('dashboardSave').disabled=!dashboardDraft||!dashboardDirty||dashboardBusy||live;renderLayoutControls();updateDashboardPreviewModeControls()}
function setDashboardBusy(busy){dashboardBusy=busy;updateDashboardDirty();if(byId('dashboardTree'))renderDashboardTree();updateDashboardPreviewModeControls()}
function dashboardLayoutNameChanged(value){dashboardLayoutName=value;updateDashboardDirty();dashboardSetBanner('Layout metadata changed. Validate before saving.','warn')}
function layoutSlug(name){let s=(name||'layout').trim().toLowerCase().normalize('NFKD').replace(/[^a-z0-9_-]+/g,'_').replace(/^_+|_+$/g,'');return s||'layout'}
async function loadLayoutCatalog(){try{const r=await fetch('/api/layouts',{cache:'no-store'});layoutCatalog=await dashboardJsonResponse(r);renderLayoutControls();return layoutCatalog}catch(e){layoutCatalog=null;renderLayoutControls();dashboardSetBanner('Could not load layout catalog: '+e.message,'error');return null}}
function renderLayoutControls(){const select=byId('layoutSelector');if(!select)return;const live=dashboardPreviewMode==='live';const current=dashboardEditingFile?'file:'+dashboardEditingFile:'inline';select.innerHTML='';if(layoutCatalog&&layoutCatalog.active&&layoutCatalog.active.type==='inline'){const o=document.createElement('option');o.value='inline';o.textContent='Inline — config.json'+(layoutCatalog.active.type==='inline'?' · active':'');select.appendChild(o)}(layoutCatalog&&Array.isArray(layoutCatalog.layouts)?layoutCatalog.layouts:[]).slice().sort((a,b)=>(a.name||a.file).localeCompare(b.name||b.file)).forEach(l=>{const o=document.createElement('option');o.value='file:'+l.file;o.disabled=l.valid===false;o.textContent=(l.name||l.file)+' — '+l.file+(l.active?' · active':'')+(l.recovered?' · recovered':'')+(l.valid===false?' · invalid':'');select.appendChild(o)});if([...select.options].some(o=>o.value===current))select.value=current;const external=!!dashboardEditingFile;byId('layoutNameWrap').classList.toggle('hidden',!external);if(external&&byId('layoutName').value!==dashboardLayoutName)byId('layoutName').value=dashboardLayoutName||'';const active=layoutCatalog&&layoutCatalog.active;byId('activeLayoutSummary').textContent=active?(active.type==='file'?'Active: '+(active.name||active.file)+' · '+active.file:'Active: Inline — config.json'):'Active source: unavailable';const recoveredConfig=!!(layoutCatalog&&layoutCatalog.active&&layoutCatalog.active.configurationRecovered);const editingEntry=external&&layoutCatalog&&Array.isArray(layoutCatalog.layouts)?layoutCatalog.layouts.find(l=>l.file===dashboardEditingFile):null;const recoveredLayout=!!(editingEntry&&editingEntry.recovered);byId('layoutSetActive').disabled=dashboardBusy||live||!external||editingLayoutIsActive()||dashboardDirty||recoveredConfig||recoveredLayout;byId('layoutUseInline').disabled=dashboardBusy||live||!external||dashboardDirty||recoveredConfig;byId('layoutUpgrade').disabled=dashboardBusy||live||!layoutCatalog||!layoutCatalog.upgradeAvailable||dashboardDirty||dashboardEditingFile!==null||!(layoutCatalog.active&&layoutCatalog.active.type==='inline')||recoveredConfig;byId('layoutNew').disabled=dashboardBusy||live||!dashboardDraft;if(byId('layoutName'))byId('layoutName').disabled=dashboardBusy||live}
function applyDashboardDocument(d,file=null,name=''){dashboardPreviewMode='configure';dashboardRuntimeState=null;clearDashboardLiveTimer();dashboardOriginal=deepClone(d);dashboardDraft=deepClone(d);dashboardEditingFile=file;dashboardLayoutName=file?(name||(d.source&&d.source.layoutName)||file):'';dashboardOriginalLayoutName=dashboardLayoutName;dashboardSelection=dashboardTiles().length?{kind:'tile',tile:0}:null;dashboardPreviewScene=null;dashboardPreviewPage=0;renderDashboard();renderLayoutControls()}
async function loadDashboard(force=false){if(dashboardDirty&&!force&&!confirm('Discard the unsaved dashboard draft and reload from the device?'))return;setDashboardBusy(true);dashboardSetBanner('Loading active dashboard configuration…');await loadLayoutCatalog();try{const r=await fetch('/api/dashboard',{cache:'no-store'});const d=await dashboardJsonResponse(r);const src=d.source||{};applyDashboardDocument(d,src.layout==='file'?src.layoutFile:null,src.layoutName||'');dashboardSetBanner(dashboardSourceSummary(),(d.warnings||[]).length?'warn':'')}catch(e){dashboardPreviewMode='configure';dashboardRuntimeState=null;clearDashboardLiveTimer();dashboardPreviewScene=null;dashboardOriginal=null;dashboardDraft=null;dashboardEditingFile=null;dashboardLayoutName='';dashboardOriginalLayoutName='';renderDashboard();const canUpgrade=!!(layoutCatalog&&layoutCatalog.upgradeAvailable&&layoutCatalog.active&&layoutCatalog.active.type==='inline');dashboardSetBanner(e.message+(canUpgrade?' Use Upgrade schema to migrate this legacy configuration.':''),'error')}finally{setDashboardBusy(false)}}
async function switchDashboardLayout(value){if(dashboardBusy)return;if(dashboardPreviewMode==='live')await setDashboardPreviewMode('configure');if(dashboardDirty&&!confirm('Discard unsaved dashboard changes and switch layouts?')){renderLayoutControls();return}if(value==='inline'){await loadDashboard(true);return}if(!value.startsWith('file:'))return;const file=value.slice(5);setDashboardBusy(true);dashboardSetBanner('Loading '+file+'…');try{const r=await fetch('/api/layout?file='+encodeURIComponent(file),{cache:'no-store'});const d=await dashboardJsonResponse(r);const item=layoutCatalog&&layoutCatalog.layouts?layoutCatalog.layouts.find(l=>l.file===file):null;applyDashboardDocument(d,file,item?item.name:(d.source&&d.source.layoutName)||file);dashboardSetBanner(dashboardSourceSummary(),(d.warnings||[]).length?'warn':'')}catch(e){dashboardSetBanner('Could not load layout: '+e.message,'error');renderLayoutControls()}finally{setDashboardBusy(false)}}
function revertDashboard(){if(!dashboardOriginal)return;dashboardDraft=deepClone(dashboardOriginal);dashboardLayoutName=dashboardOriginalLayoutName;dashboardSelection=dashboardTiles().length?{kind:'tile',tile:Math.min(dashboardSelection&&Number.isInteger(dashboardSelection.tile)?dashboardSelection.tile:0,dashboardTiles().length-1)}:null;dashboardPreviewPage=dashboardSelection?Math.floor(dashboardSelection.tile/dashboardItemsPerPage()):0;renderDashboard();renderLayoutControls();dashboardSetBanner(dashboardSourceSummary(),(dashboardDraft.warnings||[]).length?'warn':'')}
function dashboardItemsPerPage(){const n=Number(dashboardCaps().maxVisibleTiles)||6;return Math.max(1,n)}
function selectedSceneIndex(){if(!dashboardSelection)return-1;const tiles=dashboardTiles();const ti=dashboardSelection.tile;if(ti<0||ti>=tiles.length)return-1;return tiles[ti].type==='scene'?ti:-1}
function uniqueTileId(prefix){const ids=new Set(dashboardTiles().map(t=>String(t.id||'')));let base=(prefix||'tile').replace(/[^a-z0-9_-]+/gi,'-').toLowerCase()||'tile';let id=base,n=2;while(ids.has(id))id=base+'-'+n++;return id}
function newSwitch(name='New switch'){return{type:'switch',name,getTopic:'',setTopic:'',onValue:'true',offValue:'false',icon:''}}
function newSensor(name='New sensor'){return{type:'sensor',name,getTopic:'',jsondata:false,sensorType:'singleValue',firstKey:'',firstIcon:''}}
function newTile(type){const common={id:uniqueTileId(type),type,name:type==='scene'?'New scene':type==='sensor'?'New sensor':'New switch',icon:''};if(type==='switch')return Object.assign(common,newSwitch(common.name));if(type==='sensor')return Object.assign(common,newSensor(common.name));return Object.assign(common,{items:[newSwitch('Item 1'),newSwitch('Item 2')]})}
function addDashboardTile(type){if(!dashboardDraft||dashboardPreviewMode==='live')return;const allowed=dashboardCaps().tileTypes||['switch','sensor','scene'];if(!allowed.includes(type))return;const tiles=dashboardTiles();tiles.push(newTile(type));dashboardSelection={kind:'tile',tile:tiles.length-1};dashboardPreviewPage=Math.floor((tiles.length-1)/dashboardItemsPerPage());dashboardChanged()}
function addSceneItem(type){if(dashboardPreviewMode==='live')return;const ti=selectedSceneIndex();if(ti<0)return;const allowed=dashboardCaps().sceneItemTypes||['switch','sensor'];if(!allowed.includes(type))return;const items=dashboardTiles()[ti].items||(dashboardTiles()[ti].items=[]);items.push(type==='sensor'?newSensor('New sensor'):newSwitch('New switch'));dashboardSelection={kind:'item',tile:ti,item:items.length-1};dashboardChanged()}
function selectDashboard(sel){dashboardSelection=sel;if(sel&&Number.isInteger(sel.tile)){if(sel.kind==='item'){dashboardPreviewScene=sel.tile;dashboardPreviewPage=Math.floor((Number(sel.item)||0)/dashboardItemsPerPage())}else{if(dashboardPreviewScene!==null&&dashboardPreviewScene!==sel.tile)dashboardPreviewScene=null;dashboardPreviewPage=Math.floor(sel.tile/dashboardItemsPerPage())}}renderDashboard()}
function dashboardSelectionTarget(){if(!dashboardSelection)return null;const tiles=dashboardTiles();const t=tiles[dashboardSelection.tile];if(!t)return null;if(dashboardSelection.kind==='item')return t.items&&t.items[dashboardSelection.item]?t.items[dashboardSelection.item]:null;return t}
function moveDashboardSelection(delta){if(!dashboardSelection||dashboardPreviewMode==='live')return;const tiles=dashboardTiles();if(dashboardSelection.kind==='tile'){const from=dashboardSelection.tile,to=from+delta;if(to<0||to>=tiles.length)return;[tiles[from],tiles[to]]=[tiles[to],tiles[from]];if(dashboardPreviewScene===from)dashboardPreviewScene=to;else if(dashboardPreviewScene===to)dashboardPreviewScene=from;dashboardSelection.tile=to;dashboardPreviewPage=Math.floor(to/dashboardItemsPerPage())}else{const items=tiles[dashboardSelection.tile].items||[];const from=dashboardSelection.item,to=from+delta;if(to<0||to>=items.length)return;[items[from],items[to]]=[items[to],items[from]];dashboardSelection.item=to;dashboardPreviewPage=Math.floor(to/dashboardItemsPerPage())}dashboardChanged()}
function deleteDashboardSelection(){if(!dashboardSelection||dashboardPreviewMode==='live')return;const tiles=dashboardTiles();if(dashboardSelection.kind==='tile'){const ti=dashboardSelection.tile;tiles.splice(ti,1);if(dashboardPreviewScene===ti)dashboardPreviewScene=null;else if(dashboardPreviewScene!==null&&dashboardPreviewScene>ti)dashboardPreviewScene--;if(!tiles.length)dashboardSelection=null;else dashboardSelection={kind:'tile',tile:Math.min(ti,tiles.length-1)}}else{const ti=dashboardSelection.tile,items=tiles[ti].items||[],min=Number(dashboardCaps().minSceneItems)||2;if(items.length<=min){msg('A scene must contain at least '+min+' items.',false);return}const ii=dashboardSelection.item;items.splice(ii,1);dashboardSelection={kind:'item',tile:ti,item:Math.min(ii,items.length-1)};dashboardPreviewPage=Math.floor(dashboardSelection.item/dashboardItemsPerPage())}dashboardChanged()}
function convertDashboardTargetType(type){if(dashboardPreviewMode==='live')return;const target=dashboardSelectionTarget();if(!target||target.type===type)return;const common={name:target.name||'',icon:target.icon||''};if(dashboardSelection.kind==='tile'){const old=target;let replacement;if(type==='scene'){let first=old.type==='sensor'?newSensor(old.name):newSwitch(old.name);Object.keys(first).forEach(k=>{if(old[k]!==undefined)first[k]=old[k]});delete first.id;replacement={id:old.id,type:'scene',name:old.name,icon:old.icon||'',items:[first,newSwitch('Item 2')]}}else{const candidate=old.type==='scene'&&(old.items||[]).find(i=>i.type===type);replacement=type==='sensor'?newSensor(old.name):newSwitch(old.name);if(candidate)Object.keys(replacement).forEach(k=>{if(candidate[k]!==undefined)replacement[k]=candidate[k]});replacement.id=old.id;replacement.type=type;replacement.name=old.name;replacement.icon=old.icon||replacement.icon||''}dashboardTiles()[dashboardSelection.tile]=replacement}else{let replacement=type==='sensor'?newSensor(common.name):newSwitch(common.name);Object.keys(replacement).forEach(k=>{if(target[k]!==undefined)replacement[k]=target[k]});replacement.type=type;replacement.name=common.name;if(type==='switch')replacement.icon=common.icon;else delete replacement.icon;(dashboardTiles()[dashboardSelection.tile].items||[])[dashboardSelection.item]=replacement}dashboardChanged()}
function dashboardChanged(){if(dashboardPreviewScene!==null){const t=dashboardTiles()[dashboardPreviewScene];if(!t||t.type!=='scene')dashboardPreviewScene=null}renderDashboard();dashboardSetBanner('Draft changed. Validate before saving.','warn')}
function dashboardValueChanged(){renderDashboardTree();renderDashboardPreview();updateDashboardDirty();dashboardSetBanner('Draft changed. Validate before saving.','warn')}
function renderDashboard(){renderDashboardTree();renderDashboardPreview();renderDashboardInspector();updateDashboardDirty()}
function renderDashboardTree(){const root=byId('dashboardTree');const scrollTop=root.scrollTop;root.innerHTML='';const tiles=dashboardTiles(),readOnly=dashboardPreviewMode==='live';byId('dashboardCount').textContent=tiles.length+' tile'+(tiles.length===1?'':'s');const allowedTiles=dashboardCaps().tileTypes||[];byId('dashboardAddTileSwitch').disabled=readOnly||!dashboardDraft||dashboardBusy||(allowedTiles.length&&!allowedTiles.includes('switch'));byId('dashboardAddTileSensor').disabled=readOnly||!dashboardDraft||dashboardBusy||(allowedTiles.length&&!allowedTiles.includes('sensor'));byId('dashboardAddTileScene').disabled=readOnly||!dashboardDraft||dashboardBusy||(allowedTiles.length&&!allowedTiles.includes('scene'));if(!dashboardDraft){root.innerHTML='<div class="placeholder">Dashboard unavailable.</div>';return}if(!tiles.length){root.innerHTML='<div class="placeholder">No dashboard tiles. Add a switch, sensor or scene below.</div>'}tiles.forEach((tile,ti)=>{root.appendChild(makeDashboardNode(tile,ti,null));if(tile.type==='scene'&&Array.isArray(tile.items))tile.items.forEach((item,ii)=>root.appendChild(makeDashboardNode(item,ti,ii))) });const ti=selectedSceneIndex(),isScene=ti>=0;byId('dashboardAddSwitchItem').disabled=readOnly||!isScene||dashboardBusy;byId('dashboardAddSensorItem').disabled=readOnly||!isScene||dashboardBusy;const sel=dashboardSelection,target=dashboardSelectionTarget();let canUp=false,canDown=false,canDelete=!!target;if(sel&&target){if(sel.kind==='tile'){canUp=sel.tile>0;canDown=sel.tile<tiles.length-1}else{const items=tiles[sel.tile].items||[];canUp=sel.item>0;canDown=sel.item<items.length-1;canDelete=items.length>(Number(dashboardCaps().minSceneItems)||2)}}byId('dashboardMoveUp').disabled=readOnly||!canUp||dashboardBusy;byId('dashboardMoveDown').disabled=readOnly||!canDown||dashboardBusy;byId('dashboardDelete').disabled=readOnly||!canDelete||dashboardBusy;root.scrollTop=scrollTop}
function makeDashboardNode(item,ti,ii){const b=document.createElement('button');b.className='dashboard-node'+(ii===null?'':' child');const selected=dashboardSelection&&dashboardSelection.tile===ti&&((ii===null&&dashboardSelection.kind==='tile')||(ii!==null&&dashboardSelection.kind==='item'&&dashboardSelection.item===ii));if(selected)b.classList.add('selected');b.onclick=()=>selectDashboard(ii===null?{kind:'tile',tile:ti}:{kind:'item',tile:ti,item:ii});const idx=document.createElement('span');idx.className='dashboard-node-index';idx.textContent=ii===null?String(ti+1):'↳'+String(ii+1);const name=document.createElement('span');name.className='dashboard-node-name';name.textContent=item.name||'(unnamed)';const type=document.createElement('span');type.className='dashboard-node-type';type.textContent=item.type||'?';b.append(idx,name,type);return b}
function dashboardPreviewCollection(){if(dashboardPreviewScene!==null){const tile=dashboardTiles()[dashboardPreviewScene];if(tile&&tile.type==='scene'&&Array.isArray(tile.items))return tile.items;dashboardPreviewScene=null}return dashboardTiles()}
function dashboardPreviewPageCount(){return Math.max(1,Math.ceil(dashboardPreviewCollection().length/dashboardItemsPerPage()))}
function changeDashboardPreviewPage(delta){const pages=dashboardPreviewPageCount();dashboardPreviewPage=Math.max(0,Math.min(pages-1,dashboardPreviewPage+delta));renderDashboardPreview()}
function selectPreviewSceneTile(tileIndex){if(dashboardPreviewClickTimer)clearTimeout(dashboardPreviewClickTimer);dashboardPreviewClickTimer=setTimeout(()=>{dashboardPreviewClickTimer=null;selectDashboard({kind:'tile',tile:tileIndex})},220)}
function openDashboardPreviewScene(tileIndex){if(dashboardPreviewClickTimer){clearTimeout(dashboardPreviewClickTimer);dashboardPreviewClickTimer=null}const tile=dashboardTiles()[tileIndex];if(!tile||tile.type!=='scene')return;dashboardPreviewScene=tileIndex;dashboardPreviewPage=0;dashboardSelection={kind:'tile',tile:tileIndex};renderDashboard()}
function closeDashboardPreviewScene(){const tileIndex=dashboardPreviewScene;dashboardPreviewScene=null;dashboardPreviewPage=tileIndex===null?0:Math.floor(tileIndex/dashboardItemsPerPage());if(tileIndex!==null)dashboardSelection={kind:'tile',tile:tileIndex};renderDashboard()}
function addPreviewIcon(cell,path,fallback){const img=document.createElement('img');img.className='core2-tile-icon';img.alt='';img.src='/api/file?path='+encodeURIComponent(path)+'&t=preview';const fb=document.createElement('div');fb.className='core2-tile-fallback';fb.textContent=fallback;fb.style.display='none';img.onerror=()=>{img.style.display='none';fb.style.display='block'};cell.append(img,fb)}
function runtimeFreshness(runtime,isScene=false){if(!runtime)return{known:false,ageMs:null};if(!isScene)return{known:!!runtime.known,ageMs:runtime.ageMs};const items=Array.isArray(runtime.items)?runtime.items:[];if(!items.length||items.some(i=>!i||!i.known||i.ageMs===undefined))return{known:false,ageMs:null};return{known:true,ageMs:Math.max(...items.map(i=>Number(i.ageMs)||0))}}
function renderPreviewEntity(cell,entity,runtime,isScene=false){const live=dashboardPreviewMode==='live';let active=false,iconActive=false;if(entity.type==='sensor'){const combined=entity.sensorType==='combinedValues';const path=entity.firstIcon?'/'+entity.firstIcon+'.jpg':'';if(path)addPreviewIcon(cell,path,'≋');else{const fb=document.createElement('div');fb.className='core2-tile-fallback';fb.textContent='≋';cell.appendChild(fb)}const v=document.createElement('div');v.className='core2-tile-value';if(live&&runtime&&runtime.known){const first=runtime.firstValue===undefined?'-':String(runtime.firstValue);const second=runtime.secondValue===undefined?'-':String(runtime.secondValue);v.textContent=combined?first+' / '+second:first}else v.textContent=combined?'-- / --':'--';cell.appendChild(v)}else{active=live&&runtime?(isScene?!!runtime.anySwitchOn:(!!runtime.known&&!!runtime.active)):false;iconActive=live&&runtime?(isScene?!!runtime.allSwitchesOn:(!!runtime.known&&!!runtime.active)):false;const fallback=isScene?'◫':'◉';const path=entity.icon?'/'+entity.icon+'_'+(iconActive?'active':'inactive')+'.jpg':'';if(path)addPreviewIcon(cell,path,fallback);else{const fb=document.createElement('div');fb.className='core2-tile-fallback';fb.textContent=fallback;cell.appendChild(fb)}if(isScene){const more=document.createElement('span');more.className='core2-scene-more';more.textContent='…';cell.appendChild(more)}}if(active)cell.classList.add('live-active');if(live){const freshness=runtimeFreshness(runtime,isScene);const age=document.createElement('span');age.className='core2-live-age';age.textContent=freshness.known?dashboardAgeLabel(freshness.ageMs):'?';age.title=freshness.known?'Last known value age':'No runtime value received';cell.appendChild(age)}const name=document.createElement('div');name.className='core2-tile-name';name.textContent=(entity.name||'Unnamed').slice(0,15);cell.appendChild(name)}
function renderDashboardPreview(){const root=byId('dashboardPreviewGrid');root.innerHTML='';const tiles=dashboardTiles(),per=dashboardItemsPerPage();let scene=null;if(dashboardPreviewScene!==null){scene=tiles[dashboardPreviewScene];if(!scene||scene.type!=='scene'||!Array.isArray(scene.items)){dashboardPreviewScene=null;scene=null}}const collection=scene?scene.items:tiles,pages=Math.max(1,Math.ceil(collection.length/per));dashboardPreviewPage=Math.max(0,Math.min(pages-1,dashboardPreviewPage));for(let slot=0;slot<per;slot++){const index=dashboardPreviewPage*per+slot;const entity=collection[index];const cell=document.createElement('button');cell.className='core2-tile';if(entity){if(scene){const ti=dashboardPreviewScene,ii=index;if(dashboardSelection&&dashboardSelection.kind==='item'&&dashboardSelection.tile===ti&&dashboardSelection.item===ii)cell.classList.add('selected');cell.onclick=()=>selectDashboard({kind:'item',tile:ti,item:ii});renderPreviewEntity(cell,entity,dashboardPreviewMode==='live'?runtimeItemAt(ti,ii):null,false)}else{const ti=index;if(dashboardSelection&&dashboardSelection.kind==='tile'&&dashboardSelection.tile===ti)cell.classList.add('selected');if(entity.type==='scene'){cell.onclick=()=>selectPreviewSceneTile(ti);cell.ondblclick=e=>{e.preventDefault();openDashboardPreviewScene(ti)}}else cell.onclick=()=>selectDashboard({kind:'tile',tile:ti});const runtime=dashboardPreviewMode==='live'?runtimeTileAt(ti):null;renderPreviewEntity(cell,entity,entity.type==='scene'?runtime:(runtime&&runtime.items?runtime.items[0]:null),entity.type==='scene')}}else cell.disabled=true;root.appendChild(cell)}const footer=byId('dashboardPreviewFooter'),back=byId('dashboardPreviewBack'),prev=byId('dashboardPreviewPrev'),next=byId('dashboardPreviewNext');footer.classList.toggle('hidden',!scene&&pages<=1);back.classList.toggle('hidden',!scene);prev.disabled=dashboardPreviewPage<=0;next.disabled=dashboardPreviewPage>=pages-1;byId('dashboardPreviewPage').textContent=pages>1?(dashboardPreviewPage+1)+' / '+pages:'';byId('dashboardPreviewTitle').textContent=scene?(scene.name||'Scene').slice(0,24):'12:34';const liveState=dashboardPreviewMode==='live'&&dashboardRuntimeState?dashboardRuntimeState:null;const wifi=byId('dashboardPreviewWifi'),mqtt=byId('dashboardPreviewMqtt');wifi.textContent=liveState&&liveState.wifi&&!liveState.wifi.connected?'WiFi!':'WiFi';mqtt.textContent=liveState&&liveState.mqtt&&!liveState.mqtt.connected?'MQTT!':'MQTT';updateDashboardPreviewModeControls()}
function inspectorField(root,label,value,onChange,options=null){const wrap=document.createElement('div');wrap.className='inspector-field';const l=document.createElement('label');l.textContent=label;let input;if(options){input=document.createElement('select');options.forEach(o=>{const opt=document.createElement('option');opt.value=o;opt.textContent=o;input.appendChild(opt)});input.value=value||options[0]}else{input=document.createElement('input');input.type='text';input.value=value===undefined?'':value}input.addEventListener('change',()=>onChange(input.value));input.addEventListener('input',()=>{if(!options)onChange(input.value)});input.disabled=dashboardPreviewMode==='live';wrap.append(l,input);root.appendChild(wrap);return input}
function inspectorCheck(root,label,checked,onChange){const wrap=document.createElement('label');wrap.className='inspector-check';const input=document.createElement('input');input.type='checkbox';input.checked=!!checked;input.addEventListener('change',()=>onChange(input.checked));input.disabled=dashboardPreviewMode==='live';const text=document.createElement('span');text.textContent=label;wrap.append(input,text);root.appendChild(wrap)}
function renderDashboardInspector(){const root=byId('dashboardInspector');root.innerHTML='';const target=dashboardSelectionTarget();if(!target){root.innerHTML='<div class="inspector-empty">Select a tile or scene item.</div>';byId('dashboardSelectionLabel').textContent='';return}const grid=document.createElement('div');grid.className='inspector-grid';root.appendChild(grid);const tile=dashboardTiles()[dashboardSelection.tile];byId('dashboardSelectionLabel').textContent=dashboardSelection.kind==='item'?(tile.name||'Scene')+' · item '+(dashboardSelection.item+1):'tile '+(dashboardSelection.tile+1);if(dashboardSelection.kind==='tile'){inspectorField(grid,'ID',target.id||'',v=>{target.id=v;dashboardValueChanged()});inspectorField(grid,'Type',target.type,v=>convertDashboardTargetType(v),dashboardCaps().tileTypes||['switch','sensor','scene'])}else inspectorField(grid,'Type',target.type,v=>convertDashboardTargetType(v),dashboardCaps().sceneItemTypes||['switch','sensor']);inspectorField(grid,'Name',target.name||'',v=>{target.name=v;dashboardValueChanged()});if(dashboardSelection.kind==='tile'||target.type==='switch')inspectorField(grid,'Icon',target.icon||'',v=>{target.icon=v;dashboardValueChanged()});if(target.type==='switch'){inspectorField(grid,'Get topic',target.getTopic||'',v=>{target.getTopic=v;dashboardValueChanged()});inspectorField(grid,'Set topic',target.setTopic||'',v=>{target.setTopic=v;dashboardValueChanged()});inspectorField(grid,'ON value',target.onValue===undefined?'true':target.onValue,v=>{target.onValue=v;dashboardValueChanged()});inspectorField(grid,'OFF value',target.offValue===undefined?'false':target.offValue,v=>{target.offValue=v;dashboardValueChanged()})}else if(target.type==='sensor'){inspectorField(grid,'Get topic',target.getTopic||'',v=>{target.getTopic=v;dashboardValueChanged()});inspectorCheck(grid,'JSON payload',target.jsondata,v=>{target.jsondata=v;dashboardValueChanged()});inspectorField(grid,'Sensor type',target.sensorType||'singleValue',v=>{target.sensorType=v;dashboardChanged()},dashboardCaps().sensorTypes||['singleValue','combinedValues']);inspectorField(grid,'First key',target.firstKey||'',v=>{target.firstKey=v;dashboardValueChanged()});inspectorField(grid,'First icon',target.firstIcon||'',v=>{target.firstIcon=v;dashboardValueChanged()});if(target.sensorType==='combinedValues'){inspectorField(grid,'Second key',target.secondKey||'',v=>{target.secondKey=v;dashboardValueChanged()});inspectorField(grid,'Second icon',target.secondIcon||'',v=>{target.secondIcon=v;dashboardValueChanged()})}}}
async function validateDashboard(){if(!dashboardDraft)return;setDashboardBusy(true);dashboardSetBanner('Validating dashboard…');try{let url='/api/dashboard/validate';if(dashboardEditingFile)url='/api/layout/validate?file='+encodeURIComponent(dashboardEditingFile)+'&name='+encodeURIComponent(dashboardLayoutName);const r=await fetch(url,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(dashboardDraft)});const d=await dashboardJsonResponse(r);dashboardDraft=d;if(dashboardEditingFile&&d.source&&d.source.layoutName)dashboardLayoutName=d.source.layoutName;renderDashboard();renderLayoutControls();dashboardSetBanner('Draft is valid. '+dashboardSourceSummary(),(d.warnings||[]).length?'warn':'');msg('Dashboard draft is valid.',true)}catch(e){dashboardSetBanner('Validation failed: '+e.message,'error');msg('Dashboard validation failed: '+e.message,false)}finally{setDashboardBusy(false)}}
async function saveDashboard(){if(!dashboardDraft)return;if(selected&&selected.name==='/config.json'&&editorDirty&&!confirm('config.json has unsaved raw-editor changes. Dashboard Save uses the on-device config and will not include those unsaved edits. Continue?'))return;setDashboardBusy(true);dashboardSetBanner('Validating and saving dashboard…');try{let url='/api/dashboard';if(dashboardEditingFile)url='/api/layout?file='+encodeURIComponent(dashboardEditingFile)+'&name='+encodeURIComponent(dashboardLayoutName);const r=await fetch(url,{method:'PUT',headers:{'Content-Type':'application/json'},body:JSON.stringify(dashboardDraft)});const d=await dashboardJsonResponse(r);if(dashboardEditingFile&&d.source&&d.source.layoutName)dashboardLayoutName=d.source.layoutName;dashboardOriginal=deepClone(d);dashboardDraft=deepClone(d);dashboardOriginalLayoutName=dashboardLayoutName;await loadLayoutCatalog();renderDashboard();dashboardSetBanner((editingLayoutIsActive()?'Dashboard saved and runtime reload queued. ':'Layout saved without changing the active dashboard. ')+dashboardSourceSummary(),(d.warnings||[]).length?'warn':'');msg(dashboardEditingFile&&!editingLayoutIsActive()?'Layout saved.':'Dashboard saved.',true);status();loadFiles()}catch(e){dashboardSetBanner('Save failed: '+e.message,'error');msg('Dashboard save failed: '+e.message,false)}finally{setDashboardBusy(false)}}
async function createDashboardLayout(){if(!dashboardDraft||dashboardBusy)return;let name=prompt('Layout display name:','New Layout');if(!name)return;name=name.trim();if(!name)return;let file=prompt('Layout filename (no spaces):','layout_'+layoutSlug(name)+'.json');if(!file)return;file=file.trim();if(!/^layout_[A-Za-z0-9_-]+\.json$/.test(file)){msg('Filename must match layout_[A-Za-z0-9_-]+.json',false);return}if(layoutCatalog&&layoutCatalog.layouts&&layoutCatalog.layouts.some(l=>l.file===file)){msg('That layout file already exists.',false);return}setDashboardBusy(true);try{const r=await fetch('/api/layout?file='+encodeURIComponent(file)+'&name='+encodeURIComponent(name),{method:'PUT',headers:{'Content-Type':'application/json'},body:JSON.stringify(dashboardDraft)});const d=await dashboardJsonResponse(r);await loadLayoutCatalog();applyDashboardDocument(d,file,name);dashboardSetBanner('Created '+name+'. It is not active until you choose Set as active.');msg('Layout created.',true);loadFiles()}catch(e){dashboardSetBanner('Could not create layout: '+e.message,'error');msg('Could not create layout: '+e.message,false)}finally{setDashboardBusy(false)}}
async function setSelectedLayoutActive(){if(!dashboardEditingFile||dashboardBusy)return;if(dashboardDirty){msg('Save or revert this layout before activating it.',false);return}if(!confirm('Set '+(dashboardLayoutName||dashboardEditingFile)+' as the active device dashboard?'))return;setDashboardBusy(true);try{const r=await fetch('/api/dashboard/source',{method:'PUT',headers:{'Content-Type':'application/json'},body:JSON.stringify({type:'file',file:dashboardEditingFile})});const d=await dashboardJsonResponse(r);await loadLayoutCatalog();applyDashboardDocument(d,dashboardEditingFile,dashboardLayoutName);dashboardSetBanner('Layout activated. Runtime reload queued. '+dashboardSourceSummary());msg('Active dashboard changed.',true);status()}catch(e){dashboardSetBanner('Activation failed: '+e.message,'error');msg('Could not activate layout: '+e.message,false)}finally{setDashboardBusy(false)}}
async function useSelectedLayoutInline(){if(!dashboardEditingFile||dashboardBusy)return;if(dashboardDirty){msg('Save or revert this layout before copying it inline.',false);return}if(!confirm('Copy '+(dashboardLayoutName||dashboardEditingFile)+' into config.json and make the inline dashboard active? The layout file will be kept.'))return;setDashboardBusy(true);try{const r=await fetch('/api/dashboard/source',{method:'PUT',headers:{'Content-Type':'application/json'},body:JSON.stringify({type:'inline',file:dashboardEditingFile})});const d=await dashboardJsonResponse(r);await loadLayoutCatalog();applyDashboardDocument(d,null,'');dashboardSetBanner('Inline dashboard activated. Runtime reload queued. '+dashboardSourceSummary());msg('Dashboard copied inline and activated.',true);status();loadFiles()}catch(e){dashboardSetBanner('Inline activation failed: '+e.message,'error');msg('Could not activate inline dashboard: '+e.message,false)}finally{setDashboardBusy(false)}}
async function upgradeDashboardSchema(){if(!layoutCatalog||!layoutCatalog.upgradeAvailable||dashboardBusy)return;if(dashboardDirty){msg('Save or revert dashboard changes before schema upgrade.',false);return}let name=prompt('Name for the external layout created by the upgrade:','Default Dashboard');if(!name)return;name=name.trim();if(!name)return;let file=prompt('Layout filename (no spaces):','layout_'+layoutSlug(name)+'.json');if(!file)return;file=file.trim();if(!/^layout_[A-Za-z0-9_-]+\.json$/.test(file)){msg('Filename must match layout_[A-Za-z0-9_-]+.json',false);return}if(!confirm('Upgrade config.json to schema v3 and move the current dashboard into '+file+'?'))return;setDashboardBusy(true);try{const r=await fetch('/api/dashboard/upgrade',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({file,name})});const d=await dashboardJsonResponse(r);await loadLayoutCatalog();applyDashboardDocument(d,file,name);dashboardSetBanner('Schema upgraded to v3. '+file+' is now active.');msg('Configuration schema upgraded.',true);status();loadFiles()}catch(e){dashboardSetBanner('Schema upgrade failed: '+e.message,'error');msg('Schema upgrade failed: '+e.message,false)}finally{setDashboardBusy(false)}}


async function clearBootstrap(){if(confirm('Clear Wi-Fi/web bootstrap settings and reboot into setup AP?'))await post('/api/bootstrap/clear')}
async function formatFs(){if(confirm('Erase all LittleFS files including config.json?')){const r=await post('/api/fs/format');if(r.ok){clearSelection();loadFiles()}}}

function installFileManagerResize(){
 const manager=byId('fileManager');const handle=byId('fileResizeHandle');if(!manager||!handle)return;
 const compact=window.matchMedia('(max-width:780px)').matches;let saved=0;try{saved=Number(localStorage.getItem('homepointFileManagerHeight'))}catch(e){}if(!compact&&saved>=380)manager.style.height=Math.round(saved)+'px';
 let startY=0,startHeight=0;
 const move=e=>{const max=Math.max(520,window.innerHeight*1.5);const next=Math.max(380,Math.min(max,startHeight+e.clientY-startY));manager.style.height=Math.round(next)+'px'};
 const stop=e=>{window.removeEventListener('pointermove',move);window.removeEventListener('pointerup',stop);window.removeEventListener('pointercancel',stop);handle.classList.remove('dragging');try{localStorage.setItem('homepointFileManagerHeight',Math.round(manager.getBoundingClientRect().height))}catch(err){}};
 handle.addEventListener('pointerdown',e=>{if(window.matchMedia('(max-width:780px)').matches)return;e.preventDefault();startY=e.clientY;startHeight=manager.getBoundingClientRect().height;handle.classList.add('dragging');window.addEventListener('pointermove',move);window.addEventListener('pointerup',stop);window.addEventListener('pointercancel',stop)});
}

document.addEventListener('visibilitychange',()=>{if(!document.hidden&&screenAutoMs>0)refreshScreen();if(!document.hidden&&dashboardPreviewMode==='live')loadDashboardRuntimeState()});
window.addEventListener('beforeunload',e=>{if(editorDirty||dashboardDirty){e.preventDefault();e.returnValue=''}});
installFileManagerResize();identity();loadDebugUi();status();loadFiles();loadDashboard();refreshScreen();setInterval(status,5000);
</script>
</body>
</html>
)html";

}  // namespace homepoint::web
