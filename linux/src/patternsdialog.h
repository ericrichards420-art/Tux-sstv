#pragma once
#include <QDialog>
class QListWidget;
class ImageView;
class QLabel;
class PatternsDialog:public QDialog {
    Q_OBJECT
public:
    explicit PatternsDialog(const QString& selected="test-pattern",QWidget* parent=nullptr);
    QString selectedId() const;
private:
    QListWidget* patterns_;
    ImageView* preview_;
    QLabel *name_,*era_,*description_;
    void updatePreview();
};
