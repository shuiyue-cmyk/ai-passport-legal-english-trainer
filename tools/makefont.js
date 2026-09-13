'use strict';
// 本机字体重打脚本：绕开命令行传中文（Windows PowerShell 会把 UTF-8 参数转坏），
// 直接读字符集文件再调 lv_font_conv 的 convert()。
// 用法（仓库根目录）: node tools/makefont.js
// 初次先装一次转换器: cd build && npm init -y && npm i lv_font_conv@1.5.3
const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..');
const TTF = 'C:/Windows/Fonts/simhei.ttf';

function loadConvert() {
  const cands = [
    path.join(ROOT, 'build', 'node_modules', 'lv_font_conv', 'lib', 'convert'),
    path.join(__dirname, 'node_modules', 'lv_font_conv', 'lib', 'convert'),
  ];
  for (const c of cands) {
    try { return require(c); } catch (e) { /* try next */ }
  }
  console.error('找不到 lv_font_conv，先执行: cd build && npm init -y && npm i lv_font_conv@1.5.3');
  process.exit(1);
}
const convert = loadConvert();

async function makeFont({ charset, size, name, out }) {
  const symbols = fs.readFileSync(path.join(ROOT, charset), 'utf8').trim();
  const args = {
    font: [{
      source_path: TTF,
      ranges: [{ symbols }, { range: [0x20, 0x7E, 0x20] }],
      source_bin: fs.readFileSync(TTF)
    }],
    size,
    bpp: 4,
    format: 'lvgl',
    output: path.join(ROOT, out),
    lv_include: 'lvgl.h',
    lv_font_name: name,
    no_compress: true,
    opts_string: `(tools/makefont.js ${charset} size=${size})`
  };
  const files = await convert(args);
  for (const [filename, data] of Object.entries(files)) {
    fs.mkdirSync(path.dirname(filename), { recursive: true });
    fs.writeFileSync(filename, data);
    console.log('wrote', filename, fs.statSync(filename).size);
  }
}

(async () => {
  await makeFont({
    charset: 'tools/font_charset.txt', size: 16,
    name: 'vocab_cjk_16', out: 'main/fonts/vocab_cjk_16.c'
  });
  await makeFont({
    charset: 'tools/font_charset_hint.txt', size: 14,
    name: 'vocab_hint_14', out: 'main/fonts/vocab_hint_14.c'
  });
})().catch(err => { console.error(err.stack || err.message); process.exit(1); });
