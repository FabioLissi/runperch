#include "ui/themed_controls.h"
#include "ui/theme.h"
#include <QPainter>
#include <QStyleOption>
namespace runperch {
namespace {
void standardArrow(QWidget* widget,QRect area,QStyle::PrimitiveElement primitive,QPainter& painter,bool enabled=true) {
    QStyleOption arrow; arrow.initFrom(widget);
    arrow.rect=QStyle::alignedRect(widget->layoutDirection(),Qt::AlignCenter,QSize(12,12),area);
    if(!enabled) arrow.state&=~QStyle::State_Enabled;
    theme().widgetBaseStyle().drawPrimitive(primitive,&arrow,&painter,widget);
}
}
void ThemedComboBox::paintEvent(QPaintEvent* event) {
    QComboBox::paintEvent(event);
    QStyleOptionComboBox option; initStyleOption(&option); QPainter painter(this);
    standardArrow(this,style()->subControlRect(QStyle::CC_ComboBox,&option,QStyle::SC_ComboBoxArrow,this),QStyle::PE_IndicatorArrowDown,painter);
}
void ThemedSpinBox::paintEvent(QPaintEvent* event) {
    QSpinBox::paintEvent(event);
    QStyleOptionSpinBox option; initStyleOption(&option); QPainter painter(this);
    standardArrow(this,style()->subControlRect(QStyle::CC_SpinBox,&option,QStyle::SC_SpinBoxUp,this),QStyle::PE_IndicatorArrowUp,painter,option.stepEnabled&StepUpEnabled);
    standardArrow(this,style()->subControlRect(QStyle::CC_SpinBox,&option,QStyle::SC_SpinBoxDown,this),QStyle::PE_IndicatorArrowDown,painter,option.stepEnabled&StepDownEnabled);
}
}
