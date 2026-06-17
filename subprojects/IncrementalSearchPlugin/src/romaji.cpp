#include "romaji.h"

#include <cstring>
#include <vector>

struct KanaRoma {
	char const *kana;
	char const *roma;
};

static std::vector<KanaRoma> kana_roma_table = {
	{"ァ", "xa"},
	{"ア", "a"},
	{"ィ", "xi"},
	{"イ", "i"},
	{"ゥ", "xu"},
	{"ウ", "u"},
	{"ウィ", "wi"},
	{"ウェ", "we"},
	{"ェ", "xe"},
	{"エ", "e"},
	{"ォ", "xo"},
	{"オ", "o"},
	{"カ", "ka"},
	{"ガ", "ga"},
	{"キ", "ki"},
	{"キィ", "kyi"},
	{"キェ", "kye"},
	{"キャ", "kya"},
	{"キュ", "kyu"},
	{"キョ", "kyo"},
	{"ギ", "gi"},
	{"ギィ", "gyi"},
	{"ギェ", "gye"},
	{"ギャ", "gya"},
	{"ギュ", "gyu"},
	{"ギョ", "gyo"},
	{"ク", "ku"},
	{"クァ", "kwa"},
	{"クィ", "kwi"},
	{"クェ", "kwe"},
	{"クォ", "kwo"},
	{"グ", "gu"},
	{"グァ", "gwa"},
	{"グィ", "gwi"},
	{"グェ", "gwe"},
	{"グォ", "gwo"},
	{"ケ", "ke"},
	{"ゲ", "ge"},
	{"コ", "ko"},
	{"ゴ", "go"},
	{"サ", "sa"},
	{"ザ", "za"},
	{"シ", "shi"},
	{"シ", "si"},
	{"シェ", "she"},
	{"シェ", "sye"},
	{"シャ", "sha"},
	{"シャ", "sya"},
	{"シュ", "shu"},
	{"シュ", "syu"},
	{"ショ", "sho"},
	{"ショ", "syo"},
	{"ジ", "ji"},
	{"ジ", "zi"},
	{"ジィ", "jyi"},
	{"ジィ", "zyi"},
	{"ジェ", "je"},
	{"ジェ", "je"},
	{"ジェ", "jye"},
	{"ジェ", "zye"},
	{"ジャ", "ja"},
	{"ジャ", "jya"},
	{"ジャ", "zya"},
	{"ジュ", "ju"},
	{"ジュ", "jyu"},
	{"ジュ", "zyu"},
	{"ジョ", "jo"},
	{"ジョ", "jyo"},
	{"ジョ", "zyo"},
	{"ス", "su"},
	{"ズ", "zu"},
	{"セ", "se"},
	{"ゼ", "ze"},
	{"ソ", "so"},
	{"ゾ", "zo"},
	{"タ", "ta"},
	{"ダ", "da"},
	{"チ", "chi"},
	{"チ", "ti"},
	{"チェ", "che"},
	{"チェ", "che"},
	{"チェ", "tye"},
	{"チャ", "cha"},
	{"チャ", "tya"},
	{"チュ", "chu"},
	{"チュ", "tyu"},
	{"チョ", "cho"},
	{"チョ", "tyo"},
	{"ヂ", "di"},
	{"ヂェ", "dhe"},
	{"ヂェ", "dye"},
	{"ヂャ", "dha"},
	{"ヂャ", "dya"},
	{"ヂュ", "dhu"},
	{"ヂュ", "dyu"},
	{"ヂョ", "dho"},
	{"ヂョ", "dyo"},
	{"ッ", "xtu"},
	{"ツ", "tsu"},
	{"ツ", "tu"},
	{"ツァ", "tsa"},
	{"ツィ", "tsi"},
	{"ツェ", "tse"},
	{"ツォ", "tso"},
	{"ヅ", "du"},
	{"テ", "te"},
	{"ティ", "thi"},
	{"デ", "de"},
	{"ディ", "dhi"},
	{"ディ", "dhi"},
	{"ディ", "dyi"},
	{"ト", "to"},
	{"トゥ", "thu"},
	{"ド", "do"},
	{"ドゥ", "dhu"},
	{"ナ", "na"},
	{"ニ", "ni"},
	{"ニャ", "nya"},
	{"ニュ", "nyu"},
	{"ニョ", "nyo"},
	{"ヌ", "nu"},
	{"ネ", "ne"},
	{"ノ", "no"},
	{"ハ", "ha"},
	{"バ", "ba"},
	{"パ", "pa"},
	{"ヒ", "hi"},
	{"ヒィ", "hyi"},
	{"ヒェ", "hye"},
	{"ヒャ", "hya"},
	{"ヒュ", "hyu"},
	{"ヒョ", "hyo"},
	{"ビ", "bi"},
	{"ビィ", "byi"},
	{"ビェ", "bye"},
	{"ビャ", "bya"},
	{"ビュ", "byu"},
	{"ビョ", "byo"},
	{"ピ", "pi"},
	{"ピィ", "pyi"},
	{"ピェ", "pye"},
	{"ピャ", "pya"},
	{"ピュ", "pyu"},
	{"ピョ", "pyo"},
	{"フ", "fu"},
	{"フ", "hu"},
	{"ファ", "fa"},
	{"フィ", "fi"},
	{"フェ", "fe"},
	{"フォ", "fo"},
	{"フャ", "fya"},
	{"フュ", "fyu"},
	{"フョ", "fyo"},
	{"ブ", "bu"},
	{"プ", "pu"},
	{"ヘ", "he"},
	{"ベ", "be"},
	{"ペ", "pe"},
	{"ホ", "ho"},
	{"ボ", "bo"},
	{"ポ", "po"},
	{"マ", "ma"},
	{"ミ", "mi"},
	{"ミィ", "myi"},
	{"ミェ", "mye"},
	{"ミャ", "mya"},
	{"ミュ", "myu"},
	{"ミョ", "myo"},
	{"ム", "mu"},
	{"メ", "me"},
	{"モ", "mo"},
	{"ャ", "xya"},
	{"ヤ", "ya"},
	{"ュ", "xyu"},
	{"ユ", "yu"},
	{"ョ", "xyo"},
	{"ヨ", "yo"},
	{"ラ", "ra"},
	{"リ", "ri"},
	{"リィ", "ryi"},
	{"リェ", "rye"},
	{"リャ", "rya"},
	{"リュ", "ryu"},
	{"リョ", "ryo"},
	{"ル", "ru"},
	{"レ", "re"},
	{"ロ", "ro"},
	{"ヮ", "xwa"},
	{"ワ", "wa"},
	{"ヰ", "wyi"},
	{"ヱ", "wye"},
	{"ヲ", "wo"},
	{"ヴ", "vu"},
	{"ヴァ", "va"},
	{"ヴィ", "vi"},
	{"ヴェ", "ve"},
	{"ヴォ", "vo"},
	{"ヴャ", "vya"},
	{"ヴュ", "vyu"},
	{"ヴョ", "vyo"},
	{"ヵ", "xka"},
	{"ヶ", "xke"},
	{"ㇻ", "xra"},
	{"ㇼ", "xri"},
	{"ㇽ", "xru"},
	{"ㇾ", "xre"},
	{"ㇿ", "xro"},
	};

std::string convert_romaji_to_katakana(std::string_view const &romaji)
{
	std::string result;
	auto Write = [&](char const *p){
		// fwrite(p, 1, strlen(p), stdout);
		result.append(p);
	};
	
	char const *begin = romaji.data();
	char const *end = begin + romaji.size();
	char const *ptr = begin;
	while (ptr < end) {
		if (*ptr == 'n') {
			if (ptr + 1 < end && strchr("aiueoy", ptr[1])) {
				// through
			} else {
				Write("ン");
				ptr++;
				if (ptr < end && *ptr == 'n') {
					ptr++;
				}
				continue;
			}
		}
		bool match = false;
		for (KanaRoma const &item : kana_roma_table) {
			char const *roma = item.roma;
			size_t i = 0;
			while (1) {
				if (roma[i] == 0) {
					ptr += i;
					match = true;
					Write(item.kana);
					break;
				}
				if (ptr[i] == 'l' && roma[i] == 'x') {
					// through
				} else {
					if (roma[i] != ptr[i]) break;
				}
				i++;
			}
		}
		if (!match) {
			if (ptr + 1 < end) {
				int c = (unsigned char)*ptr;
				if ((c == 't' && ptr[1] == 'c') || (isalpha(c) && ptr[1] == c)) {
					Write("ッ");
					ptr++;
					continue;
				}
			}
			ptr++;
		}
	}
	
	return result;
}
