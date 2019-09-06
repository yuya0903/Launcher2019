#define NOMINMAX
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
  bool onKeyHit(int key) const {
    return !pkeyBuf[key] && keyBuf[key];
  }
  bool onKeyUp(int key) const {
    return pkeyBuf[key] && !keyBuf[key];
  }
  bool onKeyDown(int key) const {
    return keyBuf[key];
  }
  unsigned int getKeyDownTime(int key) const {
    return keyDownTime[key];
  }
};

struct Logger : public std::enable_shared_from_this<Logger> {
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

struct GameProfile {
  std::shared_ptr<Logger> logger;
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
  int detailWidth_ = 0, detailHeight = 0;
  GameProfile(std::shared_ptr<Logger> logger) :
    GameProfile(logger, fs::current_path(), TEXT("<���ݒ�>"), TEXT("�s��"), TEXT("<���ݒ�>")) {}
  GameProfile(
    std::shared_ptr<Logger> logger,
    const fs::path& dir,
    std::wstring_view title,
    std::wstring version,
    std::wstring_view description
  ) :
    GameProfile(logger, dir, title, version, description, TEXT("autorun.exe"), TEXT("icon.png"), TEXT("detail.png")) {}
  GameProfile(
    std::shared_ptr<Logger> logger,
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
    if (fs::exists(dir / icon)) {
      iconHandle = LoadGraph((dir / icon).c_str());
      if (iconHandle == NULL) {
        logger->err(icon.string() + "���J���܂���ł���");
        return;
      }
    }
    else
      logger->err(dir.string() + "����" + icon.string() + "�����݂��܂���B");
    if (fs::exists(dir / detail)) {
      detailHandle = LoadGraph((dir / detail).c_str());
      if (detailHandle == NULL) {
        logger->err(detail.string() + "���J���܂���ł���");
        return;
      }
      GetGraphSize(detailHandle, &detailWidth_, &detailHeight);
    }
    else logger->err(dir.string() + "����" + detail.string() + "�����݂��܂���B");
  }
};

std::vector<GameProfile> games;

int Init(std::shared_ptr<Logger> logger) {
  SetUseTransColor(FALSE);
  SetDoubleStartValidFlag(TRUE);
  // ChangeWindowMode(TRUE);
  SetDrawScreen(DX_SCREEN_BACK);
  SetBackgroundColor(255, 255, 255);
  if (DxLib_Init() == -1)return -1;

  games.clear();

  fs::path gameDir = fs::current_path() / TEXT("Games");
  fs::path metaFile = "settings.json";

  for (fs::recursive_directory_iterator itr(gameDir), end; itr != end; ++itr) {
    if (!itr->is_directory())continue;
    fs::path tmp = *itr / metaFile;
    if (!fs::exists(tmp)) {
      logger->err(
        "�Q�[���t�H���_ \"" + itr->path().string() + "\"�̒��Ƀt�@�C�� \""
        + metaFile.string() + "\"��������܂���ł����B"
      );
      continue;
    }
    std::wifstream ifs(tmp);
    if (!ifs.is_open()) {
      logger->err(
        "�Q�[���t�H���_ \"" + itr->path().string() + "\"���̃t�@�C�� \""
        + metaFile.string() + "\"���J���܂���ł����B"
      );
      continue;
    }
    ptree::wptree json_data;
    std::locale utf_8("ja-JP.UTF-8");
    ifs.imbue(utf_8);
    try {
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
    catch (const std::exception & e) {
      logger->info(std::string("read_json failed : ") + e.what());
    }
  }

  for (auto& v : games)v.loadImage();

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

const std::vector<std::wstring> ErrStr = {
  L"CreateProcessError",
  L"CloseHandleError",
  L"ChildProcessError",
  L"GetExitCodeError",
  L"InvalidPathError",
  L"Success"
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

int WINAPI WinMain(HINSTANCE phI, HINSTANCE hI, LPSTR cmd, int cmdShow) {

  std::ofstream ofs("error.txt");
  if (!ofs.is_open()) {
    MessageBox(GetMainWindowHandle(), TEXT("�G���[�o�͗p�t�@�C�����J���܂���ł���"), TEXT("�G���["), MB_OK);
    exit(EXIT_FAILURE);
  }
  std::shared_ptr<Logger> logger = std::make_shared<Logger>(ofs);

  InputManager in;
  in.update();

  float exrate = 0.1f;
  int curSelection = 0, prvSelection = -1;
  float selectionAngle = 0.f;
  int curPage = 0, prvPage = 0;
  bool pageChange = false;
  float pageChangeAngle = 0.f;
  int selectionX = 0;
  int selectionY = 0;
  const int SelectSpan = 3;
  const int SelectWait = 30;
  const int Rows = 2;
  const int Cols = 4;
  int ScreenWidth = 640;
  int ScreenHeight = 480;

  GetDefaultState(&ScreenWidth, &ScreenHeight, NULL);
  SetGraphMode(ScreenWidth, ScreenHeight, 32);

  int GameWidth_ = ScreenWidth / 6;
  int GameHeight = ScreenHeight / 5;
  int GameMarginTop = 10;
  int GameMarginLeft = ScreenWidth / 10;
  int GameMarginRight = ScreenWidth / 10;
  int GameMarginBottom = ScreenHeight * 2 / 5;

  int GameSpanX = (ScreenWidth - GameMarginLeft - GameMarginRight - GameWidth_) / (Cols - 1) - GameWidth_;
  int GameSpanY = (ScreenHeight - GameMarginTop - GameMarginBottom - GameHeight) / (Rows - 1) - GameHeight;

  if (Init(logger->shared_from_this()) == -1)return -1;

  int pages = games.size() / (Rows * Cols) + (games.size() % (Rows * Cols) ? 1 : 0);

  auto calc_selection = [&]() {
    curSelection = curPage * Rows * Cols + selectionY * Cols + selectionX;
  };

  int frame = 0;
  while (ProcessMessage() != -1) {
    in.update();
    if (int time = in.getKeyDownTime(KEY_INPUT_LEFT);
      (time > SelectWait&& time% SelectSpan == 0)
      || in.onKeyHit(KEY_INPUT_LEFT)) {
      if (--selectionX < 0) {
        if (curPage > 0) {
          --curPage;
          selectionX = Cols - 1;
        }
        else selectionX = 0;
      }
    }
    else if (int time = in.getKeyDownTime(KEY_INPUT_RIGHT);
      (time > SelectWait&& time% SelectSpan == 0)
      || in.onKeyHit(KEY_INPUT_RIGHT)) {
      if (++selectionX >= Cols) {
        if (curPage < pages - 1) {
          ++curPage;
          selectionX = 0;
          calc_selection();
          while (games.size() < curSelection) {
            --selectionY;
            calc_selection();
          }
        }
        else selectionX = Cols - 1;
      }
      calc_selection();
      if (curSelection >= games.size())--selectionX;
      calc_selection();
    }
    else if (int time = in.getKeyDownTime(KEY_INPUT_UP);
      (time > SelectWait&& time% SelectSpan == 0)
      || in.onKeyHit(KEY_INPUT_UP)) {
      if (selectionY > 0)--selectionY;
    }
    else if (int time = in.getKeyDownTime(KEY_INPUT_DOWN);
      (time > SelectWait&& time% SelectSpan == 0)
      || in.onKeyHit(KEY_INPUT_DOWN)) {
      if (selectionY < Rows - 1)++selectionY;
      calc_selection();
      if (curSelection >= games.size())--selectionY;
      calc_selection();
    }

    if (prvSelection != curSelection)selectionAngle = 0.f;

    if (prvPage != curPage) {
      pageChangeAngle = 0.f;
      pageChange = true;
    }
    if (pageChange && pageChangeAngle < DX_PI_F)
      pageChange += DX_PI_F / 30;
    else pageChange = false;

    if (in.onKeyHit(KEY_INPUT_RETURN)) {
      SetDxLibEndPostQuitMessageFlag(FALSE);
      DxLib_End();
      LaunchError_e err;
      DWORD exitCode = Launch(games[curSelection].dir / games[curSelection].executable, err);
      if (Init(logger->shared_from_this()) == -1)return -1;
      for (auto& v : games)v.loadImage();
      ClearDrawScreen();
      if (exitCode == -1) {
        DrawFormatString(0, 0, 0x000000, TEXT("�Q�[���̋N�����ɃG���[���������܂����B"));
        DrawFormatString(0, 16, 0x000000, TEXT("�����̐l�ɓ`���Ă��������B"));
        DrawFormatString(0, 32, 0x000000, TEXT("�Q�[���f�B���N�g���F%s"), fs::relative(games[curSelection].dir).c_str());
        DrawFormatString(0, 48, 0x000000, TEXT("�I���R�[�h�F%d, �G���[�R�[�h�F%d(%s)"), exitCode, err, ErrStr[err].c_str());
        ScreenFlip();
        while (ProcessMessage() != -1 && !in.onKeyHit(KEY_INPUT_ESCAPE))in.update();
      }
      else {
        DrawFormatString(0, 0, 0x000000, TEXT("�Q�[���͏I�����܂����B���̐l�ɑւ���Ă�������"));
        ScreenFlip();
        WaitTimer(1000 * 2);
        DrawFormatString(0, 16, 0x000000, TEXT("���ɐi�ނɂ͉����L�[�������Ă��������B"));
        ScreenFlip();
        WaitKey();
      }
      curPage = 0;
      selectionX = 0;
      selectionY = 0;
      curSelection = 0;
    }

    if (prvSelection != curSelection) {
      if (games[curSelection].is_movie)
        PlayMovieToGraph(games[curSelection].detailHandle);
      if (prvSelection >= 0 && games[prvSelection].is_movie)
        PauseMovieToGraph(games[prvSelection].detailHandle);
    }

    prvSelection = curSelection;
    prvPage = curPage;
    calc_selection();

    ClearDrawScreen();

    for (int y = 0; y < Rows; ++y) {
      for (int x = 0; x < Cols; ++x) {
        float cx = GameMarginLeft + x * (GameWidth_ + GameSpanX) + GameWidth_ / 2.f;
        float cy = GameMarginTop + y * (GameHeight + GameSpanY) + GameHeight / 2.f;
        int tmp = curPage * Rows * Cols + y * Cols + x;
        if (games.size() <= tmp)break;
        const auto& game = games[tmp];
        float rate = 1.f;
        if (curSelection == tmp)rate += sinf(selectionAngle) * exrate;
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 64);
        DrawRoundRectAA(
          cx - rate * GameWidth_ / 2.f + rate * 10,
          cy - rate * GameHeight / 2.f + rate * 10,
          cx + rate * GameWidth_ / 2.f + rate * 10,
          cy + rate * GameHeight / 2.f + rate * 10,
          GameWidth_ / 8, GameHeight / 8, 32, 0x000000, TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        DrawRoundRectAA(
          cx - rate * GameWidth_ / 2.f,
          cy - rate * GameHeight / 2.f,
          cx + rate * GameWidth_ / 2.f,
          cy + rate * GameHeight / 2.f,
          GameWidth_ / 8, GameHeight / 8, 32, 0xffffff, TRUE);
        DrawRoundRectAA(
          cx - rate * GameWidth_ / 2.f,
          cy - rate * GameHeight / 2.f,
          cx + rate * GameWidth_ / 2.f,
          cy + rate * GameHeight / 2.f,
          GameWidth_ / 8, GameHeight / 8, 32, 0x000000, FALSE);
        DrawExtendGraphF(
          cx - rate * (GameWidth_ / 2.f - GameWidth_ / 16.f),
          cy - rate * (GameHeight / 2.f - GameHeight / 16.f),
          cx + rate * (GameWidth_ / 2.f - GameWidth_ / 16.f),
          cy + rate * (GameHeight / 2.f - GameHeight / 16.f),
          game.iconHandle, TRUE);
      }
    }
    selectionAngle += DX_PI_F / 30;

    DrawFormatString(0, ScreenHeight - GameMarginBottom + GameHeight / 2.f * exrate, 0xff0000, TEXT("�^�C�g���@�F%s"), games[curSelection].title.c_str());
    DrawFormatString(0, ScreenHeight - GameMarginBottom + GameHeight / 2.f * exrate + 20, 0xff0000, TEXT("�o�[�W�����F%s"), games[curSelection].version.c_str());
    DrawFormatString(0, ScreenHeight - GameMarginBottom + GameHeight / 2.f * exrate + 40, 0xff0000, TEXT("�����F\n%s"), games[curSelection].description.c_str());

    {
      const GameProfile& game = games[curSelection];
      float x = ScreenWidth / 2.f + 5;
      float y = ScreenHeight - GameMarginBottom + GameHeight / 2.f * exrate + 5;
      float h = GameMarginBottom - GameHeight / 2.f * exrate - 10;
      float w = h * game.detailWidth_ / game.detailHeight;
      if (w > ScreenWidth / 2.f - 10) {
        w = ScreenWidth / 2.f - 10;
        h = w * game.detailHeight / game.detailWidth_;
      }
      DrawExtendGraph(x, y, x + w, y + h, game.detailHandle, TRUE);
    }

    ScreenFlip();
  }

  SetDxLibEndPostQuitMessageFlag(TRUE);
  DxLib_End();

  return 0;
}