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
.footer-grid{display:grid;grid-template-columns:1fr 1fr;gap:1rem}.message{min-height:1.3rem;margin:.5rem 0}.hidden{display:none!important}
@media(max-width:780px){body{padding:.65rem}.cards,.footer-grid{grid-template-columns:1fr}.file-manager{grid-template-columns:1fr;height:auto;min-height:0;border-radius:7px}.file-list-pane{border-right:0;border-bottom:1px solid #cfd6dc}.file-list{height:260px;flex:none}.file-detail{height:560px}.file-resize-handle,.file-resize-hint{display:none}.screen-wrap{min-height:220px}}
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
 if(!selected)return;const ta=byId('fileEditor');if(!ta)return;const path=selected.name;
 const r=await fetch('/api/file/text?path='+encodeURIComponent(path),{method:'POST',headers:{'Content-Type':'text/plain; charset=utf-8'},body:ta.value});const t=await r.text();if(r.ok){editorDirty=false;await loadFiles();selected=files.find(f=>f.name===path)||selected;byId('selectedMeta').textContent=formatBytes(new Blob([ta.value]).size)+' · '+ext(path);msg(path==='/config.json'&&!t.includes('rebooting')?t+'; use Apply configuration to activate runtime changes':t,true)}else msg(t,false)
}
function downloadSelected(){if(!selected)return;const a=document.createElement('a');a.href='/api/file?download=1&path='+encodeURIComponent(selected.name);a.download=selected.name.split('/').pop();a.click()}
async function deleteSelected(){if(!selected||selected.name==='/config.json')return;if(!confirm('Delete '+selected.name+'?'))return;const r=await fetch('/api/file?path='+encodeURIComponent(selected.name),{method:'DELETE'});msg(await r.text(),r.ok);if(r.ok){clearSelection();loadFiles()}}
async function createTextFile(){
 if(editorDirty&&!confirm('Discard unsaved changes before opening a new file?'))return;let path=prompt('New file path (for example /devices.json):','/new.json');if(!path)return;path=norm(path.trim());if(!editableExt.includes(ext(path))){msg('New editor files must use a text/JSON extension.',false);return}if(files.some(f=>f.name===path)){msg('File already exists; select it to edit or replace it explicitly.',false);return}
 const initial=ext(path)==='json'?'{\n  \n}\n':'';const r=await fetch('/api/file/text?path='+encodeURIComponent(path),{method:'POST',headers:{'Content-Type':'text/plain; charset=utf-8'},body:initial});msg(await r.text(),r.ok);if(r.ok){await loadFiles();selectFile(path)}
}
function suggestUploadPath(){const f=byId('upload').files[0];if(!f)return;byId('uploadPath').value=selected&&selected.uploadable?selected.name:norm(f.name)}
async function uploadFile(){const f=byId('upload').files[0];if(!f)return;const path=norm((byId('uploadPath').value||f.name).trim());const existing=files.find(x=>x.name===path);if(existing&&!existing.uploadable){msg('That managed file cannot be replaced through Upload.',false);return}if(path==='/config.json'){msg('Use the JSON editor for config.json so it is validated before activation.',false);return}const replacingDirty=!!(existing&&selected&&selected.name===path&&editorDirty);if(existing&&!confirm(replacingDirty?'Replace '+path+' and discard its unsaved editor changes?':'Replace existing file '+path+'?'))return;const fd=new FormData();fd.append('file',f,f.name);const r=await fetch('/api/upload?path='+encodeURIComponent(path),{method:'POST',body:fd});msg(await r.text(),r.ok);if(r.ok){if(replacingDirty)editorDirty=false;await loadFiles();if(files.some(x=>x.name===path))selectFile(path)}}
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

document.addEventListener('visibilitychange',()=>{if(!document.hidden&&screenAutoMs>0)refreshScreen()});
window.addEventListener('beforeunload',e=>{if(editorDirty){e.preventDefault();e.returnValue=''}});
installFileManagerResize();identity();loadDebugUi();status();loadFiles();refreshScreen();setInterval(status,5000);
</script>
</body>
</html>
)html";

}  // namespace homepoint::web
