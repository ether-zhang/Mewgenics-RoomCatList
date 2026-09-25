"""Compile local game descriptions into the mod. Generated game text is not tracked."""
from pathlib import Path
import csv
import io
import json
import re
from collections import Counter
from game_archive import GameArchive

ROOT = Path(__file__).resolve().parents[1]
STATS = dict(str='力量', dex='敏捷', con='体质', int='智力', spd='速度', cha='魅力', lck='幸运')
EFFECTS = dict(STATS, shield='护盾', divine_shield='神圣护盾', health_regen='生命自然恢复', mana_regen='魔力自然恢复', speed='移动范围')
EN_STATS = dict(str='Strength', dex='Dexterity', con='Constitution', int='Intelligence',
                spd='Speed', cha='Charisma', lck='Luck')
EN_EFFECTS = dict(EN_STATS, shield='Shield', divine_shield='Divine shield',
                  health_regen='Health regeneration', mana_regen='Mana regeneration', speed='Movement range')
PARTS = [(0, 'BODY'), (1, 'HEAD'), (2, 'TAIL'), (3, 'LEG'), (4, 'ARM'),
         (6, 'EYE'), (7, 'EYEBROW'), (8, 'EAR'), (9, 'MOUTH'), (10, 'FUR')]
# Explicit list/screening exception, keyed by the native ID rather than a
# translated name. The installed Chinese translation calls Chungus 大块头.
POSITIVE_DISORDERS = {'Chungus': {'zh-cn': '大块头', 'en': 'Chungus'}}


def parse_gon(text):
    tokens = re.findall(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\n|[{}\[\]]|[^\s{}\[\]"]+', text, re.S)
    tokens = [s for s in tokens if not s.startswith(('//', '/*'))]
    index = 0

    def atom(t):
        return json.loads(t) if t.startswith('"') else t

    def block(nested=False):
        nonlocal index
        result = {}
        while index < len(tokens):
            key = tokens[index]
            index += 1
            if key == '\n':
                continue
            if key == '}':
                if not nested:
                    raise ValueError('Unexpected GON close brace')
                return result
            values = []
            while index < len(tokens) and tokens[index] not in ('\n', '}'):
                value = tokens[index]
                index += 1
                if value == '{':
                    values.append(block(True))
                    break
                values.append(atom(value))
            result[atom(key)] = values[0] if len(values) == 1 else values
        if nested:
            raise ValueError('Unclosed GON block')
        return result
    return block()


def clean_text(text, language='zh-cn'):
    text = text.replace('\u200b', '').replace('\u00ad', '').replace('\\n', '\n')
    icons = dict(STATS, hp='生命', mp='魔力', shield='护盾', health='生命', mana='魔力', movement='移动',
                 comfort='舒适度', stimulation='刺激度', appeal='吸引力') if language == 'zh-cn' else dict(
                 EN_STATS, hp='Health', mp='Mana', shield='Shield', health='Health', mana='Mana',
                 movement='Movement', comfort='Comfort', stimulation='Stimulation', appeal='Appeal')
    text = re.sub(r'\[img:([^\]]+)\]', lambda m: icons.get(m[1], m[1]), text)
    text = re.sub(r'\[(?:/?b|/?i|/?u|/?color(?::[^\]]*)?|s:[^\]]+|/s)\]', '', text)
    return text.strip()


def scalar(node, key, default=''):
    value = node.get(key, default)
    return value if isinstance(value, str) else default


def is_quality_mutation(definition):
    if scalar(definition, 'tag') == 'birth_defect':
        return False
    numbers = [float(value) for field, value in definition.items() if field in EFFECTS and isinstance(value, str)]
    # A positive net sum is insufficient: even +2/-1 is not a pure gain.
    pure_gain = bool(numbers) and any(v > 0 for v in numbers) and all(v >= 0 for v in numbers)
    passives = definition.get('passives', {})
    functional = isinstance(passives, dict) and any(k != 'YOffset' and v not in ('0', 'false', 'None', '') for k, v in passives.items())
    functional |= bool(definition.get('attack') or definition.get('override_move'))
    return pure_gain or functional


def disorder_effect(definition, translate, language='zh-cn'):
    lines = []
    stats = definition.get('stats', {})
    if isinstance(stats, dict):
        for key, label in (STATS if language == 'zh-cn' else EN_STATS).items():
            value = scalar(stats, key)
            if value:
                lines.append(f'{label} {float(value):+g}')
    for key in ('shield', 'divine_shield', 'health_regen', 'mana_regen', 'speed'):
        value = scalar(definition, key)
        if value:
                lines.append(f'{(EFFECTS if language == "zh-cn" else EN_EFFECTS)[key]} {float(value):+g}')
    description = translate(scalar(definition, 'desc'))
    if description:
        lines.append(description)
    else:
        passives = definition.get('passives', {})
        if isinstance(passives, dict) and scalar(passives, 'SizeScale'):
            lines.append(f"{'体型' if language == 'zh-cn' else 'Size'} ×{float(passives['SizeScale']):g}")
    return '\n'.join(lines) or ('暂无效果说明' if language == 'zh-cn' else 'No effect description available')


def load_catalog(archive, language='zh-cn'):
    if language not in ('zh-cn', 'en'):
        raise ValueError('Unsupported trait language: ' + language)
    translations = {r['KEY']: clean_text(r.get(language) or r.get('en') or '', language)
                    for r in csv.DictReader(io.StringIO(archive.text('data/text/combined.csv')))}
    missing = set()

    def translate(key):
        if not key:
            return ''
        if key not in translations:
            missing.add(key)
        return translations.get(key, '')

    mutations, disorders, unknown = [], [], Counter()
    for name in sorted(archive.entries):
        if name.startswith('data/mutations/') and name.endswith('.gon'):
            category = Path(name).stem
            root = parse_gon(archive.text(name)).get(category, {})
            if not isinstance(root, dict):
                raise ValueError('Invalid mutation category: ' + name)
            for key, definition in root.items():
                if not re.fullmatch(r'-?\d+', key) or not isinstance(definition, dict):
                    continue
                lines = []
                for field, value in definition.items():
                    if field in EFFECTS and isinstance(value, str):
                        number = float(value)
                        lines.append(f'{(EFFECTS if language == "zh-cn" else EN_EFFECTS)[field]} {number:+g}')
                    elif field not in ('tag', 'desc', 'passives', 'name', 'override_move', 'attack'):
                        unknown[field] += 1
                description = translate(scalar(definition, 'desc'))
                if description:
                    lines.append(description)
                elif 'override_move' in definition or 'attack' in definition:
                    raise ValueError('Attack/movement override lacks its description: ' + name + ':' + key)
                delta = tuple(int(scalar(definition, stat, '0')) for stat in STATS)
                mutations.append((category, int(key), scalar(definition, 'tag') == 'birth_defect',
                                  '\n'.join(lines) or ('暂无效果说明' if language == 'zh-cn' else 'No effect description available'),
                                  delta, is_quality_mutation(definition)))
        elif name == 'data/passives/disorders.gon':
            for key, definition in parse_gon(archive.text(name)).items():
                if not isinstance(definition, dict):
                    continue
                levels = sorted(int(k) for k, v in definition.items() if k.isdigit() and isinstance(v, dict))
                # Some diseases define effects only inside numbered tiers.
                # The base entry supplies the default tier for older/zero-level slots.
                for level in [0] + levels:
                    tier = str(level or (levels[0] if levels else 0))
                    effective = dict(definition, **definition.get(tier, {}))
                    title = translate(scalar(effective, 'name')) or key
                    title = POSITIVE_DISORDERS.get(key, {}).get(language, title)
                    stats = effective.get('stats', {})
                    delta = tuple(int(scalar(stats, stat, '0')) for stat in STATS)
                    disorders.append((key, title, disorder_effect(effective, translate, language), level,
                                      delta, key in POSITIVE_DISORDERS))
    titles = [(part, translate('MUTATION_' + name + '_NAME'),
               translate('MUTATION_' + name + '_NAME_BAD')) for part, name in PARTS]
    if not mutations or not disorders or missing:
        raise ValueError('Incomplete trait catalog; missing localization: ' + ', '.join(sorted(missing)))
    return mutations, disorders, titles, unknown


def generate():
    archive = GameArchive(ROOT.parents[1] / 'resources.gpak')
    mutations, disorders, titles, unknown = load_catalog(archive)
    english_mutations, english_disorders, english_titles, english_unknown = load_catalog(archive, 'en')
    if unknown or english_unknown:
        raise ValueError('Unhandled mutation effects: ' + repr(dict(unknown or english_unknown)))
    for zh, en in zip(mutations, english_mutations):
        if zh[:3] != en[:3] or zh[4:] != en[4:]: raise ValueError('Mutation languages do not align')
    for zh, en in zip(disorders, english_disorders):
        if zh[0] != en[0] or zh[3:] != en[3:]: raise ValueError('Disorder languages do not align')
    for zh, en in zip(titles, english_titles):
        if zh[0] != en[0]: raise ValueError('Mutation titles do not align')
    if len(mutations) != len(english_mutations) or len(disorders) != len(english_disorders) or len(titles) != len(english_titles):
        raise ValueError('Trait language catalogs have different lengths')
    literal = lambda s: json.dumps(s, ensure_ascii=False)
    lines = ['// Generated from this installation. Do not commit or redistribute game text.', '#pragma once',
             'struct MutationDefinition { const char* category; int id; bool bad; const char* effect; const char* effect_en; int stat_delta[7]; bool quality; };',
             'struct DisorderDefinition { const char* id; const char* name; const char* name_en; const char* effect; const char* effect_en; int level; int stat_delta[7]; bool positive_mutation; };',
             'struct MutationTitle { unsigned part; const char* good; const char* bad; const char* good_en; const char* bad_en; };',
             'inline constexpr MutationDefinition kMutationDefinitions[] = {']
    for (category, ident, bad, effect, delta, quality), en in zip(mutations, english_mutations):
        lines.append('    {%s, %d, %s, %s, %s, {%s}, %s},' %
                     (literal(category), ident, str(bad).lower(), literal(effect), literal(en[3]),
                      ', '.join(map(str, delta)), str(quality).lower()))
    lines += ['};', 'inline constexpr DisorderDefinition kDisorderDefinitions[] = {']
    for (ident, name, effect, level, delta, positive), en in zip(disorders, english_disorders):
        lines.append('    {%s, %s, %s, %s, %s, %d, {%s}, %s},' %
                     (literal(ident), literal(name), literal(en[1]), literal(effect), literal(en[2]), level,
                      ', '.join(map(str, delta)), str(positive).lower()))
    lines += ['};', 'inline constexpr MutationTitle kMutationTitles[] = {']
    for (part, good, bad), en in zip(titles, english_titles):
        lines.append('    {%d, %s, %s, %s, %s},' % (part, literal(good), literal(bad), literal(en[1]), literal(en[2])))
    lines += ['};', '']
    (ROOT / 'src/trait_catalog.generated.hpp').write_text('\n'.join(lines), encoding='utf-8')
    print(f'Trait catalog verified: {len(mutations)} mutations, {len(disorders)} disorders, Chinese/English names and effects.')


if __name__ == '__main__':
    generate()
