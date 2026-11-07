#include "../src/steam_integration/cancel_registry_plan.hpp"
#include <cassert>
#include <string>
using namespace ww::steam_integration;
int main() {
  const std::string registry = "100\tinternal\t30\n440\tinternal\t0\tpaused\n441\tinternal\t0\n";
  auto plan = PlanCanceledQueueRemoval(registry, 440, true);
  assert(plan.state == CancelRegistryPlanState::remove_queued && plan.contents == "100\tinternal\t30\n441\tinternal\t0\n");
  assert(PlanCanceledQueueRemoval(registry, 440, false).state == CancelRegistryPlanState::blocked);
  assert(PlanCanceledQueueRemoval("440\tinternal\t99\n", 440, true).state == CancelRegistryPlanState::blocked);
  assert(PlanCanceledQueueRemoval("440\tinternal\t0\tremoving\n", 440, true).state == CancelRegistryPlanState::blocked);
  assert(PlanCanceledQueueRemoval("440\tinternal\t0\n440\tinternal\t0\n", 440, true).state == CancelRegistryPlanState::blocked);
  assert(PlanCanceledQueueRemoval("440\tinternal\t0", 440, true).state == CancelRegistryPlanState::blocked);
  assert(PlanCanceledQueueRemoval(registry, 442, true).state == CancelRegistryPlanState::unchanged);
}
