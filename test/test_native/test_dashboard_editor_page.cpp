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
  TEST_ASSERT_TRUE(contains(page, "Core2 preview"));
}

void test_admin_page_uses_whole_dashboard_api_without_live_control() {
  const char* page = homepoint::web::kAdminPage;

  TEST_ASSERT_TRUE(contains(page, "fetch('/api/dashboard'"));
  TEST_ASSERT_TRUE(contains(page, "fetch('/api/dashboard/validate'"));
  TEST_ASSERT_TRUE(contains(page, "method:'PUT'"));
  TEST_ASSERT_TRUE(contains(page, "fetch('/api/dashboard/state'"));
  TEST_ASSERT_TRUE(contains(page, "No MQTT writes are sent."));
  TEST_ASSERT_FALSE(contains(page, "/api/dashboard/tile/"));
}

void test_admin_page_supports_read_only_live_scene_navigation() {
  const char* page = homepoint::web::kAdminPage;

  TEST_ASSERT_TRUE(contains(page, "id=\"dashboardPreviewLive\""));
  TEST_ASSERT_TRUE(contains(page, "id=\"dashboardPreviewWifi\""));
  TEST_ASSERT_TRUE(contains(page, "id=\"dashboardPreviewMqtt\""));
  TEST_ASSERT_TRUE(contains(page, ">Live state</button>"));
  TEST_ASSERT_TRUE(contains(page, "openDashboardPreviewScene"));
  TEST_ASSERT_TRUE(contains(page, "closeDashboardPreviewScene"));
  TEST_ASSERT_TRUE(contains(page, "cell.ondblclick"));
  TEST_ASSERT_TRUE(contains(page, "id=\"dashboardPreviewBack\""));
  TEST_ASSERT_TRUE(contains(page, "dashboardPreviewMode==='live'"));
  TEST_ASSERT_TRUE(contains(page, "runtimeFingerprintMatchesEditor"));
  TEST_ASSERT_TRUE(contains(page, "runtimeIdentityText"));
  TEST_ASSERT_FALSE(contains(page, "runtimeSourceMatchesEditor"));
  TEST_ASSERT_FALSE(contains(page, "runtimeShapeMatchesEditor"));
  TEST_ASSERT_TRUE(contains(page, "Live state is available only for the saved active dashboard."));
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
