const canvas = document.getElementById('radar');
const ctx = canvas.getContext('2d');
const lista = document.getElementById('lista');
const nSpan = document.getElementById('n');
const staleBanner = document.getElementById('stale');

let centro = { lat: 0, lon: 0 };
let radioKm = 25;

function dibujarBase() {
  const w = canvas.width, h = canvas.height;
  const cx = w / 2, cy = h / 2;
  const rMax = Math.min(w, h) / 2 - 20;
  ctx.fillStyle = '#161b22';
  ctx.fillRect(0, 0, w, h);
  ctx.strokeStyle = '#30363d';
  ctx.fillStyle = '#8b949e';
  ctx.font = '11px system-ui';
  for (let i = 1; i <= 5; i++) {
    ctx.beginPath();
    ctx.arc(cx, cy, rMax * i / 5, 0, Math.PI * 2);
    ctx.stroke();
    ctx.fillText(Math.round(radioKm * i / 5) + ' km', cx + 4, cy - rMax * i / 5 - 2);
  }
  ctx.beginPath();
  ctx.moveTo(cx, cy - rMax); ctx.lineTo(cx, cy + rMax);
  ctx.moveTo(cx - rMax, cy); ctx.lineTo(cx + rMax, cy);
  ctx.stroke();
  ctx.fillStyle = '#79b8ff';
  ctx.fillText('N', cx - 4, cy - rMax - 4);
  ctx.fillText('S', cx - 4, cy + rMax + 14);
  ctx.fillText('E', cx + rMax + 4, cy + 4);
  ctx.fillText('O', cx - rMax - 14, cy + 4);
  ctx.fillStyle = '#f85149';
  ctx.beginPath(); ctx.arc(cx, cy, 3, 0, Math.PI * 2); ctx.fill();
}

function dibujarAviones(aviones) {
  const w = canvas.width, h = canvas.height;
  const cx = w / 2, cy = h / 2;
  const rMax = Math.min(w, h) / 2 - 20;
  for (const a of aviones) {
    if (a.dist_km > radioKm) continue;
    const r = (a.dist_km / radioKm) * rMax;
    const rad = (a.bearing - 90) * Math.PI / 180;
    const x = cx + r * Math.cos(rad);
    const y = cy + r * Math.sin(rad);
    ctx.save();
    ctx.translate(x, y);
    ctx.rotate((a.trk - 90) * Math.PI / 180);
    ctx.fillStyle = '#56d364';
    ctx.beginPath();
    ctx.moveTo(6, 0); ctx.lineTo(-4, -4); ctx.lineTo(-4, 4);
    ctx.closePath(); ctx.fill();
    ctx.restore();
    ctx.fillStyle = '#d0d7de';
    ctx.font = '10px system-ui';
    ctx.fillText(a.cs || a.hex, x + 8, y - 4);
  }
}

function pintarLista(aviones) {
  lista.innerHTML = '';
  nSpan.textContent = aviones.length;
  for (const a of aviones) {
    const tr = document.createElement('tr');
    tr.innerHTML =
      '<td>' + (a.cs || a.hex) + '</td>' +
      '<td>' + a.alt_ft + '</td>' +
      '<td>' + a.dist_km.toFixed(1) + '</td>' +
      '<td>' + a.bearing + '°</td>';
    lista.appendChild(tr);
  }
}

async function refrescar() {
  try {
    const r = await fetch('/api/aircraft');
    if (!r.ok) throw new Error('http ' + r.status);
    const data = await r.json();
    centro = data.center;
    radioKm = data.radio_km;
    staleBanner.style.display = data.stale ? 'block' : 'none';
    dibujarBase();
    dibujarAviones(data.aircraft || []);
    pintarLista(data.aircraft || []);
  } catch (e) {
    staleBanner.style.display = 'block';
    staleBanner.textContent = 'Sin conexión con la placa.';
  }
}

dibujarBase();
refrescar();
setInterval(refrescar, 2000);
