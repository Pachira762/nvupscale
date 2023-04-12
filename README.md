# nvupscale

## 概要
NVIDIA Video Effects SDKのArtifact ReductionとSuper ResolutionをAviUtlで使用するためのプラグインです。

## 前提条件
- NVIDIA VideoEffects SDKが動作すること

## インストール方法
1. [NVIDIA Broadcast ダウンロードセンター](https://www.nvidia.com/ja-jp/geforce/broadcasting/broadcast-sdk/resources/)から使用しているグラフィックカードにあったビデオエフェクトSDKをダウンロードしインストールしてください。
2. AviUtlのPluginsフォルダに *nvupscale.auf* と *nvupscale.exe* の両方を配置してください。

## ビルド方法
1. [MAXINE-VFX-SDK](https://github.com/NVIDIA/MAXINE-VFX-SDK)をクローンし *nvvfxフォルダ* を配置してください。
2. [AviUtlのお部屋](http://spring-fragrance.mints.ne.jp/aviutl/)からPluginSDKをダウンロードし *filter.h* を配置してください。
3. ビルドプラットフォームをx86に指定し*nvupscale プロジェクト*をビルドします。
4. ビルドプラットフォームをx64に指定し*nvupscale_server プロジェクト*をビルドします。
