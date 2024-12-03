#include "subscribedialog.h"
#include "ui_subscribedialog.h"
#include "typesystemrepository.h"

namespace sdt = Safir::Dob::Typesystem;

namespace
{
template <class T>
T GetHash(const QString& s)
{
    bool ok;
    auto val = s.toLongLong(&ok);
    return ok ? T(val) : T(s.toStdWString());
}
}

SubscribeDialog::SubscribeDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SubscribeDialog)
{
    ui->setupUi(this);
    setModal(true);
    this->setFixedSize(this->width(),this->height());
}

void SubscribeDialog::Show(int64_t typeId, bool registrationSub)
{
    m_typeId = typeId;
    m_registrationSub = registrationSub;

    if (m_registrationSub)
    {
        ui->hashLabel->setText("Handler");
        ui->hashLineEdit->setText("ALL_HANDLERS");
    }
    else
    {
        auto cls = TypesystemRepository::Instance().GetClass(m_typeId);
        ui->nameLabel->setText(cls->name);

        if (cls->dobBaseClass == TypesystemRepository::Entity)
        {
            ui->hashLabel->setText("Instance");
            ui->hashLineEdit->setText("ALL_INSTANCES");
        }
        else if (cls->dobBaseClass == TypesystemRepository::Message)
        {
            ui->hashLabel->setText("Channel");
            ui->hashLineEdit->setText("ALL_CHANNELS");
        }
    }

    show();
}

SubscribeDialog::~SubscribeDialog()
{
    delete ui;
}

int64_t SubscribeDialog::TypeId() const
{
    return m_typeId;
}

bool SubscribeDialog::RegistrationsSubscription() const
{
    return m_registrationSub;
}

bool SubscribeDialog::IncludeSubclasses() const
{
    return ui->recursiveCheckBox->isChecked();
}

Safir::Dob::Typesystem::ChannelId SubscribeDialog::Channel() const
{
    auto s = ui->hashLineEdit->text();
    if (s.isEmpty())
    {
        return sdt::ChannelId::ALL_CHANNELS;
    }

    return GetHash<sdt::ChannelId>(s);
}

Safir::Dob::Typesystem::HandlerId SubscribeDialog::Handler() const
{
    auto s = ui->hashLineEdit->text();
    if (s.isEmpty())
    {
        return sdt::HandlerId::ALL_HANDLERS;
    }

    return GetHash<sdt::HandlerId>(s);
}

Safir::Dob::Typesystem::InstanceId SubscribeDialog::Instance() const
{
    auto s = ui->hashLineEdit->text();
    if (s.isEmpty())
    {
        return sdt::InstanceId();
    }

    return GetHash<sdt::InstanceId>(s);
}
