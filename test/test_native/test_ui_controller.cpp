#include <unity.h>

#include "../../src/ui/UiController.h"

namespace {

using homepoint::core::HapticCue;
using homepoint::core::ScreenPowerState;
using homepoint::core::ScreenTouchPhase;
using homepoint::core::TileAction;
using homepoint::core::TileGesture;
using homepoint::core::UiIntent;
using homepoint::core::UiNavigationAction;
using homepoint::model::AppConfig;
using homepoint::model::SensorDevice;
using homepoint::model::Tile;
using homepoint::model::TileItem;
using homepoint::model::TileItemType;
using homepoint::model::TileType;
using homepoint::ui::UiCommandKind;
using homepoint::ui::UiController;
using homepoint::ui::UiLayoutMetrics;
using homepoint::ui::UiScreen;

TileItem switchItem(bool active) {
  TileItem item;
  item.type = TileItemType::Switch;
  item.switchDevice.active = active;
  return item;
}

TileItem sensorItem() {
  TileItem item;
  item.type = TileItemType::Sensor;
  item.sensorDevice = SensorDevice{};
  return item;
}

Tile switchTile(bool active) {
  Tile tile;
  tile.type = TileType::Switch;
  tile.items.push_back(switchItem(active));
  return tile;
}

Tile sensorTile() {
  Tile tile;
  tile.type = TileType::Sensor;
  tile.items.push_back(sensorItem());
  return tile;
}

Tile sceneTile(std::initializer_list<bool> states) {
  Tile tile;
  tile.type = TileType::Scene;
  for (const bool state : states) tile.items.push_back(switchItem(state));
  return tile;
}

AppConfig configWithSimpleTiles(std::size_t count) {
  AppConfig config;
  for (std::size_t i = 0; i < count; ++i) {
    config.tiles.push_back(switchTile(false));
  }
  return config;
}

void goToSecondHomePage(UiController& controller) {
  const auto result =
      controller.handleIntent(UiIntent::navigate(UiNavigationAction::Next));
  TEST_ASSERT_TRUE(result.handled);
  TEST_ASSERT_EQUAL_UINT32(1, controller.homePage());
}

}  // namespace

void test_ui_controller_home_paging_and_navigation_bounds() {
  auto config = configWithSimpleTiles(7);
  UiController controller;
  controller.begin(&config, false, {6, 6});

  TEST_ASSERT_EQUAL_UINT32(2, controller.homePageCount());
  TEST_ASSERT_EQUAL_UINT32(0, controller.homePage());
  TEST_ASSERT_FALSE(controller.navigationEnabled(UiNavigationAction::Previous));
  TEST_ASSERT_TRUE(controller.navigationEnabled(UiNavigationAction::Next));

  auto result = controller.handleIntent(
      UiIntent::navigate(UiNavigationAction::Next));
  TEST_ASSERT_TRUE(result.handled);
  TEST_ASSERT_TRUE(result.navigationChanged);
  TEST_ASSERT_TRUE(result.hasHapticCue);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(HapticCue::Press),
      static_cast<int>(result.hapticCue));
  TEST_ASSERT_EQUAL_UINT32(1, controller.homePage());

  result = controller.handleIntent(
      UiIntent::navigate(UiNavigationAction::Next));
  TEST_ASSERT_FALSE(result.handled);
  TEST_ASSERT_EQUAL_UINT32(1, controller.homePage());

  TEST_ASSERT_TRUE(controller.navigationEnabled(UiNavigationAction::Previous));
  TEST_ASSERT_FALSE(controller.navigationEnabled(UiNavigationAction::Next));
}

void test_ui_controller_back_returns_to_originating_home_page() {
  auto config = configWithSimpleTiles(7);
  UiController controller;
  controller.begin(&config, false, {6, 6});
  goToSecondHomePage(controller);

  auto result = controller.handleIntent(
      UiIntent::tileGestureIntent(6, TileGesture::LongPress));
  TEST_ASSERT_TRUE(result.navigationChanged);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(UiScreen::TileDetail),
      static_cast<int>(controller.screen()));
  TEST_ASSERT_EQUAL_UINT32(6, controller.selectedTile());

  result = controller.handleIntent(
      UiIntent::navigate(UiNavigationAction::Back));
  TEST_ASSERT_TRUE(result.handled);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(UiScreen::Home),
      static_cast<int>(controller.screen()));
  TEST_ASSERT_EQUAL_UINT32(1, controller.homePage());
}

void test_ui_controller_home_action_returns_to_first_page() {
  auto config = configWithSimpleTiles(7);
  UiController controller;
  controller.begin(&config, false, {6, 6});
  goToSecondHomePage(controller);

  const auto result = controller.handleIntent(
      UiIntent::navigate(UiNavigationAction::Home));
  TEST_ASSERT_TRUE(result.handled);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(UiScreen::Home),
      static_cast<int>(controller.screen()));
  TEST_ASSERT_EQUAL_UINT32(0, controller.homePage());
}

void test_ui_controller_short_switch_press_emits_target_state_command() {
  AppConfig config;
  config.tiles.push_back(switchTile(false));
  config.tiles.push_back(switchTile(true));

  UiController controller;
  controller.begin(&config, false, {6, 6});

  auto result = controller.handleIntent(
      UiIntent::tilePressStarted(0));
  TEST_ASSERT_TRUE(result.handled);
  TEST_ASSERT_TRUE(result.hasHapticCue);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(HapticCue::Press),
      static_cast<int>(result.hapticCue));

  result = controller.handleIntent(
      UiIntent::tileGestureIntent(0, TileGesture::ShortPress));
  TEST_ASSERT_TRUE(result.handled);
  TEST_ASSERT_FALSE(result.navigationChanged);
  TEST_ASSERT_FALSE(result.hasHapticCue);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(TileAction::Toggle),
      static_cast<int>(result.tileAction));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(UiCommandKind::SetTileSwitchState),
      static_cast<int>(result.command.kind));
  TEST_ASSERT_EQUAL_UINT32(0, result.command.tileIndex);
  TEST_ASSERT_TRUE(result.command.targetOn);

  result = controller.handleIntent(
      UiIntent::tileGestureIntent(1, TileGesture::ShortPress));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(UiCommandKind::SetTileSwitchState),
      static_cast<int>(result.command.kind));
  TEST_ASSERT_FALSE(result.command.targetOn);
}

void test_ui_controller_scene_toggle_uses_all_switch_state() {
  AppConfig config;
  config.tiles.push_back(sceneTile({true, false}));
  config.tiles.push_back(sceneTile({true, true}));

  UiController controller;
  controller.begin(&config, false, {6, 6});

  auto result = controller.handleIntent(
      UiIntent::tileGestureIntent(0, TileGesture::ShortPress));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(UiCommandKind::SetTileSwitchState),
      static_cast<int>(result.command.kind));
  TEST_ASSERT_TRUE(result.command.targetOn);

  result = controller.handleIntent(
      UiIntent::tileGestureIntent(1, TileGesture::ShortPress));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(UiCommandKind::SetTileSwitchState),
      static_cast<int>(result.command.kind));
  TEST_ASSERT_FALSE(result.command.targetOn);
}

void test_ui_controller_long_press_opens_detail_and_requests_long_haptic() {
  AppConfig config;
  config.tiles.push_back(switchTile(false));

  UiController controller;
  controller.begin(&config, false, {6, 6});

  const auto result = controller.handleIntent(
      UiIntent::tileGestureIntent(0, TileGesture::LongPress));
  TEST_ASSERT_TRUE(result.handled);
  TEST_ASSERT_TRUE(result.navigationChanged);
  TEST_ASSERT_TRUE(result.hasHapticCue);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(HapticCue::LongPress),
      static_cast<int>(result.hapticCue));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(TileAction::OpenDetail),
      static_cast<int>(result.tileAction));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(UiCommandKind::None),
      static_cast<int>(result.command.kind));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(UiScreen::TileDetail),
      static_cast<int>(controller.screen()));
  TEST_ASSERT_EQUAL_UINT32(0, controller.selectedTile());
}

void test_ui_controller_sensor_short_press_opens_detail_without_command() {
  AppConfig config;
  config.tiles.push_back(sensorTile());

  UiController controller;
  controller.begin(&config, false, {6, 6});

  const auto result = controller.handleIntent(
      UiIntent::tileGestureIntent(0, TileGesture::ShortPress));
  TEST_ASSERT_TRUE(result.handled);
  TEST_ASSERT_TRUE(result.navigationChanged);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(TileAction::OpenDetail),
      static_cast<int>(result.tileAction));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(UiCommandKind::None),
      static_cast<int>(result.command.kind));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(UiScreen::TileDetail),
      static_cast<int>(controller.screen()));
}

void test_ui_controller_detail_item_toggle_emits_item_command() {
  AppConfig config;
  config.tiles.push_back(sceneTile({false, true}));

  UiController controller;
  controller.begin(&config, false, {6, 6});

  auto result = controller.handleIntent(UiIntent::toggleTileItem(0, 0));
  TEST_ASSERT_TRUE(result.handled);
  TEST_ASSERT_TRUE(result.hasHapticCue);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(HapticCue::Press),
      static_cast<int>(result.hapticCue));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(UiCommandKind::SetTileItemSwitchState),
      static_cast<int>(result.command.kind));
  TEST_ASSERT_EQUAL_UINT32(0, result.command.tileIndex);
  TEST_ASSERT_EQUAL_UINT32(0, result.command.itemIndex);
  TEST_ASSERT_TRUE(result.command.targetOn);

  result = controller.handleIntent(UiIntent::toggleTileItem(0, 1));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(UiCommandKind::SetTileItemSwitchState),
      static_cast<int>(result.command.kind));
  TEST_ASSERT_FALSE(result.command.targetOn);
}

void test_ui_controller_rejects_non_switch_and_out_of_range_items() {
  AppConfig config;
  Tile tile;
  tile.type = TileType::Scene;
  tile.items.push_back(sensorItem());
  config.tiles.push_back(tile);

  UiController controller;
  controller.begin(&config, false, {6, 6});

  auto result = controller.handleIntent(UiIntent::toggleTileItem(0, 0));
  TEST_ASSERT_FALSE(result.handled);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(UiCommandKind::None),
      static_cast<int>(result.command.kind));

  result = controller.handleIntent(UiIntent::toggleTileItem(0, 99));
  TEST_ASSERT_FALSE(result.handled);

  result = controller.handleIntent(
      UiIntent::tileGestureIntent(99, TileGesture::ShortPress));
  TEST_ASSERT_FALSE(result.handled);
}

void test_ui_controller_detail_paging_respects_layout_metrics() {
  AppConfig config;
  Tile tile;
  tile.type = TileType::Scene;
  for (int i = 0; i < 7; ++i) tile.items.push_back(switchItem(false));
  config.tiles.push_back(tile);

  UiController controller;
  controller.begin(&config, false, {6, 6});
  controller.handleIntent(
      UiIntent::tileGestureIntent(0, TileGesture::LongPress));

  TEST_ASSERT_TRUE(controller.navigationEnabled(UiNavigationAction::Next));
  TEST_ASSERT_FALSE(controller.navigationEnabled(UiNavigationAction::Previous));

  auto result = controller.handleIntent(
      UiIntent::navigate(UiNavigationAction::Next));
  TEST_ASSERT_TRUE(result.handled);
  TEST_ASSERT_EQUAL_UINT32(1, controller.detailPage());
  TEST_ASSERT_TRUE(controller.navigationEnabled(UiNavigationAction::Previous));
  TEST_ASSERT_FALSE(controller.navigationEnabled(UiNavigationAction::Next));

  result = controller.handleIntent(
      UiIntent::navigate(UiNavigationAction::Next));
  TEST_ASSERT_FALSE(result.handled);
  TEST_ASSERT_EQUAL_UINT32(1, controller.detailPage());
}

void test_ui_controller_footer_visibility_tracks_navigation_context() {
  AppConfig oneTile;
  oneTile.tiles.push_back(switchTile(false));

  UiController controller;
  controller.begin(&oneTile, false, {6, 6});
  TEST_ASSERT_FALSE(controller.footerVisible());

  controller.handleIntent(
      UiIntent::tileGestureIntent(0, TileGesture::LongPress));
  TEST_ASSERT_TRUE(controller.footerVisible());

  auto manyTiles = configWithSimpleTiles(7);
  controller.setConfig(&manyTiles, {6, 6});
  TEST_ASSERT_TRUE(controller.footerVisible());
}

void test_ui_controller_display_mode_change_does_not_wake_sleeping_screen() {
  auto config = configWithSimpleTiles(1);
  UiController controller;
  controller.begin(&config, false, {6, 6});
  controller.stepScreenPower(ScreenTouchPhase::None, true);

  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ScreenPowerState::Off),
      static_cast<int>(controller.screenPowerState()));

  controller.setDebugMode(true, {6, 6});
  TEST_ASSERT_TRUE(controller.debugMode());
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ScreenPowerState::Off),
      static_cast<int>(controller.screenPowerState()));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(UiScreen::Home),
      static_cast<int>(controller.screen()));
}

void test_ui_controller_config_replacement_resets_navigation_and_wakes_screen() {
  auto original = configWithSimpleTiles(7);
  auto replacement = configWithSimpleTiles(1);
  UiController controller;
  controller.begin(&original, false, {6, 6});
  goToSecondHomePage(controller);
  controller.handleIntent(
      UiIntent::tileGestureIntent(6, TileGesture::LongPress));
  controller.stepScreenPower(ScreenTouchPhase::None, true);

  controller.setConfig(&replacement, {6, 6});

  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(UiScreen::Home),
      static_cast<int>(controller.screen()));
  TEST_ASSERT_EQUAL_UINT32(0, controller.homePage());
  TEST_ASSERT_EQUAL_UINT32(0, controller.detailPage());
  TEST_ASSERT_EQUAL_UINT32(0, controller.selectedTile());
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ScreenPowerState::Awake),
      static_cast<int>(controller.screenPowerState()));
}

void test_ui_controller_layout_change_clamps_invalid_pages() {
  auto config = configWithSimpleTiles(13);
  UiController controller;
  controller.begin(&config, false, {6, 6});

  controller.handleIntent(UiIntent::navigate(UiNavigationAction::Next));
  controller.handleIntent(UiIntent::navigate(UiNavigationAction::Next));
  TEST_ASSERT_EQUAL_UINT32(2, controller.homePage());
  TEST_ASSERT_EQUAL_UINT32(3, controller.homePageCount());

  controller.setLayoutMetrics({10, 6});
  TEST_ASSERT_EQUAL_UINT32(2, controller.homePageCount());
  TEST_ASSERT_EQUAL_UINT32(0, controller.homePage());
}

void test_ui_controller_message_state_round_trips() {
  auto config = configWithSimpleTiles(1);
  UiController controller;
  controller.begin(&config, false, {6, 6});

  controller.setMessage("hello");
  TEST_ASSERT_EQUAL_STRING("hello", controller.message().c_str());

  controller.clearMessage();
  TEST_ASSERT_EQUAL_STRING("", controller.message().c_str());
}
