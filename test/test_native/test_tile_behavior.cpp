#include <unity.h>

#include "TileInteraction.h"

void test_tile_behavior_resolves_gestures_independently_of_tile_type() {
  using homepoint::core::TileAction;
  using homepoint::core::TileBehavior;
  using homepoint::core::TileGesture;
  using homepoint::core::resolveTileAction;

  const TileBehavior dimmerLike{
      TileAction::Toggle,
      TileAction::OpenDetail,
  };

  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(TileAction::Toggle),
      static_cast<int>(resolveTileAction(dimmerLike, TileGesture::ShortPress)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(TileAction::OpenDetail),
      static_cast<int>(resolveTileAction(dimmerLike, TileGesture::LongPress)));
}
