#!/usr/bin/env python3
"""Regenerate the embedded Emoji 17.0 catalog from ICU 78 and CLDR/IBus.

Maintainer-only dependencies: libicu78, python3-gi, gir1.2-ibus-1.0 and ibus-data.
No build/runtime dependency on these Python modules is introduced.
Unicode data/annotations: Unicode License v3 (resources/licenses/UNICODE.txt).
"""
import ctypes as c
import json
from pathlib import Path
import gi

gi.require_version('IBus', '1.0')
from gi.repository import IBus

ROOT = Path(__file__).resolve().parents[1]
CATEGORIES = ['Smileys & Emotion', 'People & Body', 'Animals & Nature', 'Food & Drink',
              'Travel & Places', 'Activities', 'Objects', 'Symbols', 'Flags', 'Components']
# Recent CLDR annotations can precede IBus's category table.
NEW_CATEGORIES = {
    '🫩': 'Smileys & Emotion', '\U0001faea': 'Smileys & Emotion', '\U0001faef': 'Smileys & Emotion',
    '\U0001fac8': 'People & Body', '🧑\u200d🩰': 'People & Body', '🫆': 'People & Body',
    '\U0001facd': 'Animals & Nature', '🪾': 'Animals & Nature', '🫜': 'Food & Drink',
    '\U0001f6d8': 'Travel & Places', '\U0001fa8a': 'Objects', '🪉': 'Objects',
    '\U0001fa8e': 'Objects', '🪏': 'Objects', '🫟': 'Symbols', '🇨🇶': 'Flags',
}
ALIASES = {'👋': 'hello goodbye wave', '📻': 'ham amateur radio',
           '📡': 'ham amateur radio antenna satellite dish', '🛰️': 'ham amateur radio satellite',
           '🏕️': 'portable camping radio', '🏡': 'home house', '🇺🇸': 'USA US America',
           '🇬🇧': 'UK Britain British', '😀': 'grinning smile happy'}

def without_tones(symbol):
    return ''.join(ch for ch in symbol if not 0x1f3fb <= ord(ch) <= 0x1f3ff)

def rgi_emoji():
    library = c.CDLL('libicuuc.so.78')
    def api(name, args, result):
        function = getattr(library, name + '_78')
        function.argtypes, function.restype = args, result
        return function
    open_set = api('uset_openPattern', [c.POINTER(c.c_uint16), c.c_int32, c.POINTER(c.c_int32)], c.c_void_p)
    count = api('uset_getItemCount', [c.c_void_p], c.c_int32)
    item = api('uset_getItem', [c.c_void_p, c.c_int32, c.POINTER(c.c_int32), c.POINTER(c.c_int32), c.POINTER(c.c_uint16), c.c_int32, c.POINTER(c.c_int32)], c.c_int32)
    close = api('uset_close', [c.c_void_p], None)
    pattern = '[:RGI_Emoji:]'.encode('utf-16-le')
    units = (c.c_uint16 * (len(pattern) // 2)).from_buffer_copy(pattern)
    error = c.c_int32(0)
    handle = open_set(units, len(units), c.byref(error))
    assert handle and error.value <= 0, error.value
    result = set()
    try:
        for index in range(count(handle)):
            start, end, error = c.c_int32(), c.c_int32(), c.c_int32(0)
            buffer = (c.c_uint16 * 64)()
            length = item(handle, index, c.byref(start), c.byref(end), buffer, 64, c.byref(error))
            assert error.value <= 0, error.value
            if length:
                result.add(bytes(buffer)[:length * 2].decode('utf-16-le'))
            else:
                result.update(chr(cp) for cp in range(start.value, end.value + 1))
    finally:
        close(handle)
    assert len(result) == 3953, 'Expected Emoji 17.0 RGI set (3944 emoji + 9 components)'
    return result

def main():
    allowed = rgi_emoji()
    annotations = IBus.EmojiData.load('/usr/share/ibus/dicts/emoji-en.dict')
    source = {entry.get_emoji(): entry for entry in annotations}
    entries = []
    for symbol in allowed:
        entry = source.get(symbol)
        if entry:
            name, category, keywords = entry.get_description(), entry.get_category(), entry.get_annotations()
        else:
            # Emoji 17 added these twenty one-tone gendered sequences; use
            # the base annotation plus the matching CLDR skin-tone name.
            base = source[without_tones(symbol)]
            tones = {ch for ch in symbol if 0x1f3fb <= ord(ch) <= 0x1f3ff}
            assert len(tones) == 1
            tone = source[next(iter(tones))].get_description()
            name, category, keywords = base.get_description() + ': ' + tone, base.get_category(), base.get_annotations() + [tone]
        category = NEW_CATEGORIES.get(without_tones(symbol), category)
        if not category:
            assert any(0x1f3fb <= ord(ch) <= 0x1f3ff for ch in symbol), (symbol, name)
            category = 'People & Body'
        if category == 'Component': category = 'Components'
        assert category in CATEGORIES, (symbol, name, category)
        keywords = sorted(set(keywords + ALIASES.get(without_tones(symbol), '').split()))
        entries.append({'symbol': symbol, 'name': name, 'category': category, 'keywords': ' '.join(keywords)})
    # Keep the CLDR keyboard order supplied by IBus within each category.
    order = {entry.get_emoji(): index for index, entry in enumerate(annotations)}
    entries.sort(key=lambda e: (CATEGORIES.index(e['category']), order.get(e['symbol'], order.get(without_tones(e['symbol']), 100000)), e['symbol']))
    data = {'unicodeVersion': '17.0', 'license': 'Unicode-3.0', 'source': 'ICU 78.2 RGI_Emoji / Unicode CLDR English annotations via IBus', 'categories': CATEGORIES, 'entries': entries}
    path = ROOT / 'resources/emoji_catalog.json'
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n')
    print(f'Wrote {len(entries)} Unicode Emoji 17.0 entries to {path}')

if __name__ == '__main__': main()
