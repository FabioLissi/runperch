#pragma once
#include <QComboBox>
#include <QSpinBox>
namespace runperch {
// Keep Qt input, accessibility and popup behavior; only restore the standard
// arrow primitives that custom stylesheet subcontrols otherwise suppress.
class ThemedComboBox final : public QComboBox {
public:
    using QComboBox::QComboBox;
protected:
    void paintEvent(QPaintEvent*) override;
};
class ThemedSpinBox final : public QSpinBox {
public:
    using QSpinBox::QSpinBox;
protected:
    void paintEvent(QPaintEvent*) override;
};
}
