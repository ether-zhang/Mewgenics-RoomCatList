import sys
import struct
import unittest
from pathlib import Path
from PIL import ImageFont

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from build_ui_theme import NativeUi,read_swf
from game_archive import GameArchive
from swf_font import font_glyphs


class NativeThemeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.archive=GameArchive(ROOT.parents[1]/'resources.gpak')
        cls.ui=NativeUi(cls.archive)

    def test_artwork_is_from_original_confirmation_states(self):
        root=self.ui.symbols['ConfirmationBox']
        button=next(child for child,_,_,name in self.ui.frame(root,index=0) if name=='yes')
        for state in ('up','over','down','disabled'):
            groups=self.ui.collect(button,label=state)
            self.assertTrue(any(fill[0]=='bitmap' for fill,*_ in groups))
        self.assertEqual(sum(k==82 for k,_,_ in self.ui.tags),1)

    def test_native_font_cache_has_valid_checksum_and_chinese(self):
        data=(ROOT/'build/assets/native-ui.ttf').read_bytes()
        self.assertEqual(data[:4],b'\x00\x01\x00\x00')
        padded=data+b'\0'*((-len(data))%4)
        self.assertEqual(sum(struct.unpack('>'+'I'*(len(padded)//4),padded))&0xFFFFFFFF,0xB1B0AFBA)
        font=ImageFont.truetype(str(ROOT/'build/assets/native-ui.ttf'),22)
        self.assertGreater(font.getlength('猫咪列表'),40)
        self.assertGreater(font.getbbox('桑葚确定处理')[3],font.getbbox('桑葚确定处理')[1])
        body=next(b for k,b,_ in read_swf(self.archive,'swfs/international_fonts.swf') if k==75 and struct.unpack_from('<H',b)[0]==4)
        glyphs,_,_=font_glyphs(body)
        self.assertTrue(all(ord(c) in glyphs for c in '猫咪筛选设置遗传属性。，：；'))

if __name__=='__main__':unittest.main()
