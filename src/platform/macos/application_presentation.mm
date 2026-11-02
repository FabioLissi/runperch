#include "platform/application_presentation.h"
#import <AppKit/AppKit.h>

namespace runperch {
namespace {
class MacApplicationPresentation final : public ApplicationPresentation {
public:
    bool supportsDockVisibility() const override { return true; }
    bool setDockVisible(bool visible) override {
        if(![NSThread isMainThread]) return false;
        return [[NSApplication sharedApplication] setActivationPolicy:visible
            ? NSApplicationActivationPolicyRegular : NSApplicationActivationPolicyAccessory];
    }
};
}
std::unique_ptr<ApplicationPresentation> createApplicationPresentation() {
    return std::make_unique<MacApplicationPresentation>();
}
}
