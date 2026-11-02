#include "platform/application_presentation.h"
namespace runperch {
std::unique_ptr<ApplicationPresentation> createApplicationPresentation() {
    return std::make_unique<ApplicationPresentation>();
}
}
