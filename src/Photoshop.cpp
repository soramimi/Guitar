#include "Photoshop.h"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

inline uint32_t read_uint32_be(void const *p)
{
	auto const *q = (uint8_t const *)p;
	return (q[0] << 24) | (q[1] << 16) | (q[2] << 8) | q[3];
}

inline uint16_t read_uint16_be(void const *p)
{
	auto const *q = (uint8_t const *)p;
	return (q[0] << 8) | q[1];
}

} // namespace

/**
 * \brief       PhotoshopのPSDファイルからJPEG形式のサムネイル画像を抽出する
 * \param[in]   in      入力データを読み込むQIODevice
 * \param[out]  jpeg    抽出されたJPEG形式のサムネイル画像データを格納するstd::vector<char>
 *
 * 指定されたQIODeviceからPhotoshopのPSDファイルを読み込み、
 * JPEG形式のサムネイル画像データをstd::vector<char>に格納します。
 * サムネイル画像が見つからない場合や、入力データが無効な場合には、
 * JPEGデータはクリアされます。
 */
void photoshop::readThumbnail(QIODevice *in, std::vector<char> *jpeg)
{
	// JPEGデータをクリア
	jpeg->clear();

	// Photoshopファイルヘッダ構造体
	struct FileHeader {
		char sig[4];      // シグネチャ（8BPS）
		char ver[2];      // バージョン
		char reserved[6]; // 予約領域
		char channels[2]; // チャンネル数
		char height[4];   // 画像の高さ
		char width[4];    // 画像の幅
		char depth[2];    // ビット深度
		char colormode[2];// カラーモード
	};
	FileHeader fh;

	// ファイルヘッダを読み込む
	in->read((char *)&fh, sizeof(FileHeader));

	// シグネチャが8BPSであることを確認
	if (memcmp(fh.sig, "8BPS", 4) == 0) {
		char tmp[4];
		uint32_t len;

		// 各ブロックの長さを読み込む
		in->read(tmp, 4);
		len = read_uint32_be(tmp);

		// ブロックをスキップ
		in->seek(in->pos() + len);
		in->read(tmp, 4);
		len = read_uint32_be(tmp);
		(void)len;

		// イメージリソースヘッダを検索
		while (1) {
			struct ImageResourceHeader {
				char sig[4]; // シグネチャ（8BIM）
				char id[2];  // リソースID
			};
			ImageResourceHeader irh;

			// イメージリソースヘッダを読み込む
			in->read((char *)&irh, sizeof(ImageResourceHeader));

			// シグネチャが8BIMであることを確認
			if (memcmp(irh.sig, "8BIM", 4) == 0) {
				std::vector<char> name;

				// イメージリソース名を読み込む
				while (1) {
					char c;
					if (in->read(&c, 1) != 1) break;
					if (c == 0) {
						if ((name.size() & 1) == 0) {
							if (in->read(&c, 1) != 1) break;
						}
						break;
					}
					name.push_back(c);
				}

				// ブロックの長さを読み込む
				in->read(tmp, 4);
				len = read_uint32_be(tmp);

				// 現在の位置を保存
				qint64 pos = in->pos();

				// リソースIDを読み込む
				uint16_t resid = read_uint16_be(irh.id);

				// サムネイル画像リソースの場合
				if (resid == 0x0409 || resid == 0x040c) {
					struct ThumbnailResourceHeader {
						uint32_t format;                 // サムネイル形式
						uint32_t width;                  // サムネイルの幅
						uint32_t height;                 // サムネイルの高さ
						uint32_t widthbytes;             // 1行あたりのバイト数
						uint32_t totalsize;              // サムネイルデータの総バイト数
						uint32_t size_after_compression; // 圧縮後のサイズ
						uint16_t bits_per_pixel;         // 1ピクセルあたりのビット数
						uint16_t num_of_planes;          // プレーン数
					};
					ThumbnailResourceHeader trh;

					// サムネイルリソースヘッダを読み込む
					in->read((char *)&trh, sizeof(ThumbnailResourceHeader));

					// サムネイルがJPEG形式の場合
					if (read_uint32_be(&trh.format) == 1) {
						uint32_t size_after_compression = read_uint32_be(&trh.size_after_compression);

						// サムネイルデータのサイズが適切な範囲内である場合
						if (size_after_compression < 1000000) {
							jpeg->resize(size_after_compression);
							char *ptr = &jpeg->at(0);

							// サムネイルデータを読み込む
							if (in->read(ptr, size_after_compression) == size_after_compression) {
								// 正常に読み込まれた場合は処理終了
							} else {
								// 読み込みに失敗した場合、JPEGデータをクリア
								jpeg->clear();
							}
						}
					}
					break;
				}

				// 次のイメージリソースに移動
				if (len & 1) len++;
				in->seek(pos + len);
			} else {
				break;
			}
		}
	}
}
