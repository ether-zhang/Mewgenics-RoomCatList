from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from build_trait_catalog import parse_gon, clean_text, load_catalog, is_quality_mutation, GameArchive, ROOT


class TraitCatalogTests(unittest.TestCase):
    def test_quality_requires_no_numeric_loss(self):
        self.assertTrue(is_quality_mutation({'str': '2', 'dex': '1'}))
        self.assertFalse(is_quality_mutation({'str': '2', 'dex': '-1'}))
        self.assertFalse(is_quality_mutation({'str': '20', 'dex': '-1'}))
        self.assertFalse(is_quality_mutation({'str': '0'}))
        self.assertFalse(is_quality_mutation({'str': '2', 'tag': 'birth_defect'}))
        self.assertTrue(is_quality_mutation({'cha': '-1', 'passives': {'Thorns': '2'}}))
        self.assertFalse(is_quality_mutation({'passives': {'YOffset': '2'}}))

    def test_nested_gon_and_quoted_braces(self):
        result = parse_gon('body {\n// ignored {\n300 { con 1 }\n301 {\ndesc "A {name} // B"\npassives { Thorns 1 }\n}\n}')
        self.assertEqual(result['body']['300']['con'], '1')
        self.assertEqual(result['body']['301']['desc'], 'A {name} // B')
        self.assertEqual(result['body']['301']['passives']['Thorns'], '1')

    def test_localized_effect_markup(self):
        self.assertEqual(clean_text('力\u200b量 [img:str]+2\\n[b]说明[/b]'), '力量 力量+2\n说明')
        self.assertEqual(clean_text('[s:.7][img:comfort]+1[/s]'), '舒适度+1')

    def test_installed_definitions(self):
        mutations, diseases, titles, unknown = load_catalog(GameArchive(ROOT.parents[1] / 'resources.gpak'))
        self.assertFalse(unknown)
        definitions = {(category, ident): (bad, effect) for category, ident, bad, effect, delta, quality in mutations}
        deltas = {(category, ident): delta for category, ident, bad, effect, delta, quality in mutations}
        quality = {(category, ident): quality for category, ident, bad, effect, delta, quality in mutations}
        self.assertTrue(quality['body', 300])
        self.assertFalse(quality['body', 303])
        self.assertTrue(quality['body', 314])
        self.assertEqual(deltas['body', 300], (0, 0, 1, 0, 0, 0, 0))
        self.assertEqual(deltas['body', 303][4], -2)
        self.assertEqual(deltas['legs', 705][4], 0) # Movement range isn't the SPD attribute.
        self.assertEqual(definitions['body', 300], (False, '体质 +1'))
        self.assertFalse(definitions['body', 303][0]) # Negative speed is not a birth defect.
        self.assertTrue(definitions['legs', 705][0])
        self.assertIn('神圣护盾 +1', definitions['ears', 340][1])
        rabies = next(d for d in diseases if d[0] == 'Rabies')
        self.assertEqual(rabies[1], '狂犬病')
        self.assertNotIn('[img:', rabies[2])
        self.assertTrue(all(good and bad for _, good, bad in titles))
        self.assertTrue(all(effect and effect != '暂无效果说明' for _, _, effect, *_ in diseases))
        definitions = {(ident, level): (name, effect) for ident, name, effect, level, *_ in diseases}
        self.assertIn('魅力 +8', definitions['Gigachad', 0][1])
        self.assertIn('体质 -2', definitions['ScalyScabs', 0][1])
        self.assertIn('护盾 +8', definitions['ScalyScabs', 0][1])
        self.assertIn('10%', definitions['DejaVu', 1][1])
        self.assertIn('史蒂文', definitions['DejaVu', 3][1])
        self.assertIn('250%', definitions['Distemper', 10][1])

    def test_chungus_is_the_only_positive_disorder(self):
        _, disorders, _, _ = load_catalog(GameArchive(ROOT.parents[1] / 'resources.gpak'))
        positives = [d for d in disorders if d[5]]
        self.assertEqual(positives, [('Chungus', '大块头', '体质 +4', 0, (0, 0, 4, 0, 0, 0, 0), True)])
        # Other stat boosts do not grant a blanket disease exemption.
        definitions = {d[0]: d for d in disorders}
        for ident in ('Gigantism', 'Gargantuan', 'Gigachad', 'FattyLiver', 'Touched', 'Rabies'):
            self.assertFalse(definitions[ident][5])

    def test_english_catalog_tracks_the_same_native_ids_and_effects(self):
        archive = GameArchive(ROOT.parents[1] / 'resources.gpak')
        zh_mutations, zh_disorders, zh_titles, _ = load_catalog(archive)
        en_mutations, en_disorders, en_titles, _ = load_catalog(archive, 'en')
        self.assertEqual([(m[0],m[1],m[2],m[4:]) for m in zh_mutations],
                         [(m[0],m[1],m[2],m[4:]) for m in en_mutations])
        self.assertEqual([(d[0],d[3:]) for d in zh_disorders],
                         [(d[0],d[3:]) for d in en_disorders])
        self.assertEqual([t[0] for t in zh_titles], [t[0] for t in en_titles])
        english = {d[0]:d for d in en_disorders}
        self.assertEqual(english['Chungus'][1:3], ('Chungus','Constitution +4'))
        self.assertIn('Strength', english['Rabies'][2])
        self.assertIn('Constitution +1', next(m[3] for m in en_mutations if m[0:2] == ('body',300)))


if __name__ == '__main__':
    unittest.main()
