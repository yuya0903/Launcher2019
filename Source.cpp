#include "DxLib.h"
#include <iostream>
#include <fstream>
#include <array>
#include <filesystem>
#include <cassert>

#include <boost/property_tree/json_parser.hpp>

namespace fs = std::filesystem;
namespace ptree = boost::property_tree;

class InputManager {
private:
  char keyBuf[256] = {};
  char pkeyBuf[256] = {};
  unsigned int keyDownTime[256] = {};
public:
  void update() {
    memcpy_s(pkeyBuf, 256, keyBuf, 256);
    GetHitKeyStateAll(keyBuf);
    for (int i = 0; i < 256; ++i) {
      if (pkeyBuf[i] && keyBuf[i])++keyDownTime[i];
      else keyDownTime[i] = 0;
    }
  }
  bool onKeyHit(int key) const { return !pkeyBuf[key] && keyBuf[key]; }
  bool onKeyUp(int key) const { return pkeyBuf[key] && !keyBuf[key]; }
  bool onKeyDown(int key) const { return keyBuf[key]; }
  unsigned int getKeyDownTime(int key) const { return keyDownTime[key]; }
};

int Init() {
  SetDoubleStartValidFlag(TRUE);
  ChangeWindowMode(TRUE);
  SetDrawScreen(DX_SCREEN_BACK);
  if (DxLib_Init() == -1)return -1;
  return 0;
}

enum LaunchError_e {
  CreateProcessError = 0,
  CloseHandleError,
  ChildProcessError,
  GetExitCodeError,
  InvalidPathError,
  Success
};

int Launch(const fs::path& path, LaunchError_e& err) {
  if (!fs::is_regular_file(path)) {
    err = InvalidPathError;
    return -1;
  }
  STARTUPINFO si;
  PROCESS_INFORMATION pi;
  HANDLE childProcess;

  memset(&si, 0, sizeof STARTUPINFO);
  si.cb = sizeof STARTUPINFO;

  DWORD exitCode = -1;

  if (!CreateProcess(path.c_str(), NULL, NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS, NULL, path.parent_path().c_str(), &si, &pi)) {
    err = CreateProcessError;
    return -1;
  }

  childProcess = pi.hProcess;

  if (!CloseHandle(pi.hThread)) {
    err = CloseHandleError;
    return -1;
  }
  DWORD r = WaitForSingleObject(childProcess, INFINITE);
  switch (r) {
  case WAIT_OBJECT_0:
    break;
  default:
    err = ChildProcessError;
    return -1;
    break;
  }
  if (!GetExitCodeProcess(childProcess, &exitCode)) {
    err = GetExitCodeError;
    return -1;
  }
  err = Success;
  return exitCode;
}

struct Logger {
  std::basic_ostream<char>& ost;
  Logger(std::basic_ostream<char>& ostream) : ost(ostream) {}
  void err(const std::string& message) {
    ost << "[ERROR]" << message << std::endl;
  }
  void info(const std::string& message) {
    ost << "[INFO]" << message << std::endl;
  }
  void debug(const std::string& message) {
    ost << "[DEBUG]" << message << std::endl;
  }
};

int WINAPI WinMain(HINSTANCE phI, HINSTANCE hI, LPSTR cmd, int cmdShow) {

  std::ofstream ofs("error.txt");
  if (!ofs.is_open()) {
    MessageBox(GetMainWindowHandle(), TEXT("�G���[�o�͗p�t�@�C�����J���܂���ł���"), TEXT("�G���["), MB_OK);
    exit(EXIT_FAILURE);
  }

  Logger logger(ofs);

  InputManager in;
  in.update();

  int selection = 0;
  const int SelectSpan = 3;
  const int SelectWait = 30;
  const int ColumnNum = 4;
  const int RowNum = 4;
  const int ScreenWidth = 640;
  const int ScreenHeight = 480;

  fs::path gameDir = fs::current_path() / TEXT("Games");
  fs::path metaFile = "settings.json";
  struct GameProfile {
    Logger& logger;
    fs::path dir;
    std::wstring title;
    std::wstring version;
    std::wstring description;
    fs::path executable;
    fs::path icon;
    int iconHandle;
    fs::path detail;
    int detailHandle;
    bool is_movie;
    GameProfile(Logger& logger) :
      GameProfile(logger, fs::current_path(), TEXT("title unspecified"), TEXT("-1"), TEXT("description is not available")) {}
    GameProfile(
      Logger& logger,
      const fs::path& dir,
      std::wstring_view title,
      std::wstring version,
      std::wstring_view description
    ) :
      GameProfile(logger, dir, title, version, description, TEXT("autorun.exe"), TEXT("icon.png"), TEXT("detail.png")) {}
    GameProfile(
      Logger& logger,
      const fs::path& dir,
      std::wstring_view title,
      std::wstring version,
      std::wstring_view description,
      const fs::path& executable,
      const fs::path& icon,
      const fs::path& detail,
      bool is_movie = false
    ) :
      logger(logger),
      dir(dir),
      title(title),
      version(version),
      description(description),
      executable(executable),
      icon(icon),
      detail(detail) {}
    void loadImage() {
      if (fs::exists(dir / icon))
        iconHandle = LoadGraph((dir / icon).c_str());
      else
        logger.err(dir.string() + "����" + icon.string() + "�����݂��܂���B");
      if (fs::exists(dir / detail))
        detailHandle = LoadGraph((dir / detail).c_str());
    }
  };
  std::vector<GameProfile> games;
  for (fs::recursive_directory_iterator itr(gameDir), end; itr != end; ++itr) {
    if (!itr->is_directory())continue;
    fs::path tmp = *itr / metaFile;
    if (!fs::exists(tmp)) {
      logger.err(
        "�Q�[���t�H���_ \"" + itr->path().string() + "\"�̒��Ƀt�@�C�� \""
        + metaFile.string() + "\"��������܂���ł����B"
      );
      continue;
    }
    std::wifstream ifs(tmp);
    if (!ifs.is_open()) {
      logger.err(
        "�Q�[���t�H���_ \"" + itr->path().string() + "\"���̃t�@�C�� \""
        + metaFile.string() + "\"���J���܂���ł����B"
      );
      continue;
    }
    ptree::wptree json_data;
    std::locale utf_8("ja-JP.UTF-8");
    ifs.imbue(utf_8);
    ptree::json_parser::read_json(ifs, json_data);
    GameProfile profile(logger);
    profile.dir = *itr;
    if (auto opt = json_data.get_child_optional(TEXT("title")))
      profile.title = opt->get_value<std::wstring>();
    if (auto opt = json_data.get_child_optional(TEXT("version")))
      profile.version = opt->get_value<std::wstring>();
    if (auto opt = json_data.get_child_optional(TEXT("description")))
      profile.description = opt->get_value<std::wstring>();
    if (auto opt = json_data.get_child_optional(TEXT("executable")))
      profile.executable = opt->get_value<std::wstring>();
    if (auto opt = json_data.get_child_optional(TEXT("icon")))
      profile.icon = opt->get_value<std::wstring>();
    if (auto opt = json_data.get_child_optional(TEXT("detail"))) {
      if (auto opt2 = opt->get_child_optional(TEXT("file")))
        profile.detail = opt2->get_value<std::wstring>();
      if (auto opt2 = opt->get_child_optional(TEXT("is_movie")))
        profile.is_movie = opt2->get_value<bool>();
    }
    games.push_back(profile);
  }

  if (Init() == -1)return -1;
  for (auto& v : games)v.loadImage();

  int frame = 0;
  while (ProcessMessage() != -1 && !in.onKeyHit(KEY_INPUT_ESCAPE)) {
    in.update();

    if (int time = in.getKeyDownTime(KEY_INPUT_UP);
      (time > SelectWait && time % SelectSpan == 0)
      || in.onKeyHit(KEY_INPUT_UP)) {
      if (games[selection].is_movie)
        PauseMovieToGraph(games[selection].detailHandle);
      --selection;
      if (selection < 0)selection = games.size() - 1;
      if (games[selection].is_movie) {
        SeekMovieToGraph(games[selection].detailHandle, 0);
        PlayMovieToGraph(games[selection].detailHandle, DX_PLAYTYPE_NORMAL);
      }
    }

    if (int time = in.getKeyDownTime(KEY_INPUT_DOWN);
      (time > SelectWait && time % SelectSpan == 0)
      || in.onKeyHit(KEY_INPUT_DOWN)) {
      if (games[selection].is_movie)
        PauseMovieToGraph(games[selection].detailHandle);
      ++selection;
      if (selection >= games.size())selection = 0;
      if (games[selection].is_movie) {
        SeekMovieToGraph(games[selection].detailHandle, 0);
        PlayMovieToGraph(games[selection].detailHandle, DX_PLAYTYPE_NORMAL);
      }
    }


    if (in.onKeyHit(KEY_INPUT_RETURN)) {
      SetDxLibEndPostQuitMessageFlag(FALSE);
      DxLib_End();
      LaunchError_e err;
      DWORD exitCode = Launch(games[selection].dir / games[selection].executable, err);
      if (Init() == -1)return -1;
      for (auto& v : games)v.loadImage();
      ClearDrawScreen();
      if (exitCode == -1) {
        DrawFormatString(0, 0, 0xffffff, TEXT("�Q�[���̋N�����ɃG���[���������܂����B"));
        DrawFormatString(0, 16, 0xffffff, TEXT("�����̐l�ɓ`���Ă��������B"));
        DrawFormatString(0, 32, 0xffffff, TEXT("�I���R�[�h�F%d, �G���[�R�[�h�F%d"), exitCode, err);
        ScreenFlip();
        while (ProcessMessage() != -1 && !in.onKeyHit(KEY_INPUT_ESCAPE));
        break;
      } else {
        DrawFormatString(0, 0, 0xffffff, TEXT("�Q�[���͏I�����܂����B���̐l�ɑւ���Ă�������"));
        ScreenFlip();
        WaitTimer(1000 * 2);
        DrawFormatString(0, 16, 0xffffff, TEXT("���ɐi�ނɂ͂Ȃɂ��L�[�������Ă��������B"));
        ScreenFlip();
        WaitKey();
      }
    }

    ClearDrawScreen();
    for (int y = 0; y < RowNum; ++y) {
      for (int x = 0; x < ColumnNum; ++x) {
        int n = y * 5 + x;
        if (n >= games.size())break;
        if (n == selection) {
          DrawExtendGraph(
            x * ScreenWidth / ColumnNum, y * ScreenHeight / RowNum,
            (x + 1) * ScreenWidth / ColumnNum, (y + 1) * ScreenHeight / RowNum,
            games[n].detailHandle, TRUE
          );
        } else {
          DrawExtendGraph(
            x * ScreenWidth / ColumnNum, y * ScreenHeight / RowNum,
            (x + 1) * ScreenWidth / ColumnNum, (y + 1) * ScreenHeight / RowNum,
            games[n].iconHandle, TRUE
          );

        }
      }
    }
    {
      int w = selection % ColumnNum, h = selection / RowNum;
      DrawBox(
        w * ScreenWidth / ColumnNum, h * ScreenHeight / RowNum,
        (w + 1) * ScreenWidth / ColumnNum, (h + 1) * ScreenHeight / RowNum,
        0xff0000, FALSE
      );
    }
    for (int i = 0; i < games.size(); ++i) {
      DrawFormatString(0, i * 16, i == selection ? 0xff0000 : 0xffffff, TEXT("%d : %s"), i, games[i].title.c_str());
    }
    DrawFormatString(0, 300, 0xff0000, games[selection].description.c_str());
    DrawFormatString(0, 320, 0xff0000, games[selection].executable.c_str());
    ScreenFlip();
  }

  SetDxLibEndPostQuitMessageFlag(TRUE);
  DxLib_End();

  return 0;
}