#pragma once
#include "contactlog.h"
#include <QDialog>
// The editor holds snapshots; only Save writes the contact and its pictures.
class ContactEditor:public QDialog {
    Q_OBJECT
public:
    ContactEditor(sstv::ContactLog log,sstv::Contact seed,QImage rx,QImage tx,QWidget* parent=nullptr);
};
class ContactLogDialog:public QDialog {
    Q_OBJECT
public:
    ContactLogDialog(sstv::ContactLog log,QWidget* parent=nullptr);
};
