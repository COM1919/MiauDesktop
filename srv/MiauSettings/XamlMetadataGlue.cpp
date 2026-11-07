// XamlMetadataGlue.cpp
// VS2026 移除了 UWP C++ 工具，WinUI3 C++ 工程中 XamlMetaDataProvider 的
// "把 cppwinrt 生成文件组合成编译单元"这一步缺失，这里手工补上。
// 说明：
//   - XamlMetaDataProvider.h   由 XAML 编译器生成（位于 $(IntDir)），含类声明与工厂
//   - XamlMetaDataProvider.g.cpp 由 cppwinrt 生成（位于 Generated Files），含 winrt_make_ 与委托构造
//   - 三个接口方法的方法体由 XamlTypeInfo.Impl.g.cpp 生成
#include "pch.h"
#include "XamlMetaDataProvider.h"
#include "XamlMetaDataProvider.g.cpp"
