#include <unity.h>

#include "UiChange.h"

void test_ui_change_preserves_model_item_coordinates() {
  const auto change = homepoint::core::UiChange::modelItem(3, 7);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(homepoint::core::UiChangeKind::ModelItem),
      static_cast<int>(change.kind));
  TEST_ASSERT_EQUAL_UINT32(3, change.tileIndex);
  TEST_ASSERT_EQUAL_UINT32(7, change.itemIndex);
}
