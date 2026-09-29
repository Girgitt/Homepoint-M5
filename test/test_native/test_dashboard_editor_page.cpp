#include <unity.h>

#include <cstring>

#ifndef PROGMEM
#define PROGMEM
#endif
#include "../../src/web/Pages.h"

namespace {

bool contains(const char* text, const char* needle) {
  return std::strstr(text, needle) != nullptr;
}

}  // namespace

void test_admin_page_contains_hybrid_dashboard_editor() {
  const char* page = homepoint::web::kAdminPage;

  TEST_ASSERT_TRUE(contains(page, "id=\"dashboardCard\""));
  TEST_ASSERT_TRUE(contains(page, "id=\"dashboardTree\""));
  TEST_ASSERT_TRUE(contains(page, "id=\"dashboardScreen\""));
  TEST_ASSERT_TRUE(contains(page, "id=\"dashboardInspector\""));
  TEST_ASSERT_TRUE(contains(page, "Simulated Core2 screen"));
}

void test_admin_page_uses_whole_dashboard_api_without_live_control() {
  const char* page = homepoint::web::kAdminPage;

  TEST_ASSERT_TRUE(contains(page, "fetch('/api/dashboard'"));
  TEST_ASSERT_TRUE(contains(page, "fetch('/api/dashboard/validate'"));
  TEST_ASSERT_TRUE(contains(page, "method:'PUT'"));
  TEST_ASSERT_TRUE(contains(page, "Preview state is simulated and never controls MQTT"));
  TEST_ASSERT_FALSE(contains(page, "/api/dashboard/tile/"));
}

void test_admin_page_contains_layout_source_management() {
  const char* page = homepoint::web::kAdminPage;

  TEST_ASSERT_TRUE(contains(page, "id=\"layoutSelector\""));
  TEST_ASSERT_TRUE(contains(page, "id=\"layoutName\""));
  TEST_ASSERT_TRUE(contains(page, "Upgrade schema"));
  TEST_ASSERT_TRUE(contains(page, "Set as active"));
  TEST_ASSERT_TRUE(contains(page, "Use inline copy"));
  TEST_ASSERT_TRUE(contains(page, "fetch('/api/layouts'"));
  TEST_ASSERT_TRUE(contains(page, "fetch('/api/layout?file='"));
  TEST_ASSERT_TRUE(contains(page, "'/api/layout/validate?file='"));
  TEST_ASSERT_TRUE(contains(page, "fetch('/api/dashboard/source'"));
  TEST_ASSERT_TRUE(contains(page, "fetch('/api/dashboard/upgrade'"));
  TEST_ASSERT_TRUE(contains(
      page,
      "await loadLayoutCatalog();try{const r=await fetch('/api/dashboard'"));
  TEST_ASSERT_TRUE(contains(
      page,
      "Use Upgrade schema to migrate this legacy configuration."));
}
