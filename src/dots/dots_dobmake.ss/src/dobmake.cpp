/******************************************************************************
*
* Copyright Consoden AB, 2014 (http://www.consoden.se)
*
* Created by: Lars Hagström / lars.hagstrom@consoden.se
*
*******************************************************************************
*
* This file is part of Safir SDK Core.
*
* Safir SDK Core is free software: you can redistribute it and/or modify
* it under the terms of version 3 of the GNU General Public License as
* published by the Free Software Foundation.
*
* Safir SDK Core is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with Safir SDK Core.  If not, see <http://www.gnu.org/licenses/>.
*
******************************************************************************/
#include "dobmake.h"
#include <iostream>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning (disable: 4127)
#pragma warning (disable: 4244)
#pragma warning (disable: 4251)
#endif

#include "ui_dobmake.h"
#include <QFileDialog>
#include <QProcess>
#include <QMessageBox>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

Dobmake::Dobmake(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::Dobmake)
{
    ui->setupUi(this);
    ui->douDirectory->setText("");
    ui->installDirectory->setText("");

#if defined(linux) || defined(__linux) || defined(__linux__)
    ui->configCheckButtons->hide();
    m_debug = false;
    m_release = true;
#elif defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
    ui->configRadioButtons->hide();
    m_debug = true;
    m_release = true;
#else
#  error Dobmake does not know how to handle this platform
#endif
}

Dobmake::~Dobmake()
{
    delete ui;
}


bool Dobmake::CheckPython()
{
    QStringList params;
    params << "--version";

    QProcess p;
    p.setStandardOutputFile(QProcess::nullDevice());
    p.setStandardErrorFile(QProcess::nullDevice());
    p.start("python", params);
    p.waitForFinished(-1);

    return p.error() == QProcess::UnknownError && p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
}

QString Dobmake::GetDobmakeBatchScript()
{
#if defined(linux) || defined(__linux) || defined(__linux__)
    const QString separator = ":";
    const QString scriptSuffix = "";
#elif defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
    const QString separator = ";";
    const QString scriptSuffix = ".py";
#else
#  error Dobmake does not know how to handle this platform
#endif

    const QString pathenv = QProcessEnvironment::systemEnvironment().value("PATH");
    const QStringList paths = pathenv.split(separator,QString::SkipEmptyParts);

    for (QStringList::const_iterator it = paths.begin();
         it != paths.end(); ++it)
    {
        QFile f(*it + QDir::separator() + "dobmake-batch" + scriptSuffix);
        if (f.exists())
        {
            return f.fileName();
        }
    }
    return "";
}

void Dobmake::on_douDirectoryBrowse_clicked()
{
    QFileDialog dialog;
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setOption(QFileDialog::ShowDirsOnly);

    if (dialog.exec())
    {
        ui->douDirectory->setText(dialog.selectedFiles()[0]);
    }
}

void Dobmake::on_douDirectory_textChanged(const QString &path)
{
    const QFile dir(path);
    const QFile cmakelists(path + QDir::separator() + "CMakeLists.txt");
    ui->build->setEnabled(dir.exists() && cmakelists.exists());
    if (dir.exists() && cmakelists.exists())
    {
        ui->douDirectory->setStyleSheet("");
    }
    else
    {
        ui->douDirectory->setStyleSheet("Background-color:red");
    }

    UpdateInstallButton();
}

void Dobmake::on_installDirectory_textChanged(const QString &path)
{
    const QFile dir(path);
    //ui->build->setEnabled(cmakelists.exists());
    if (dir.exists())
    {
        ui->installDirectory->setStyleSheet("");
    }
    else
    {
        ui->installDirectory->setStyleSheet("Background-color:red");
    }

    UpdateInstallButton();
}

void Dobmake::on_installDirectoryBrowse_clicked()
{
    QFileDialog dialog;
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setOption(QFileDialog::ShowDirsOnly);

    if (dialog.exec())
    {
        ui->installDirectory->setText(dialog.selectedFiles()[0]);
    }
}

void Dobmake::UpdateInstallButton()
{
    const QFile buildDir(ui->douDirectory->text());
    const QFile cmakelists(ui->douDirectory->text() + QDir::separator() + "CMakeLists.txt");
    const QFile installDir(ui->installDirectory->text());
    ui->buildAndInstall->setEnabled(buildDir.exists() && cmakelists.exists() && installDir.exists());
}

void Dobmake::on_build_clicked()
{
    QStringList params;

    params << GetDobmakeBatchScript();
    params << "--skip-tests";

#if defined(linux) || defined(__linux) || defined(__linux__)
    params << "--config";
#elif defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
    params << "--configs"
#else
#  error Dobmake does not know how to handle this platform
#endif

    if (m_debug)
    {
        params << "Debug";
    }

    if (m_release)
    {
        params << "Release";
    }
    std::wcout << params.join(" ").toStdWString() << std::endl;
    QProcess p;
    p.setWorkingDirectory(ui->douDirectory->text());
    p.setStandardOutputFile(QProcess::nullDevice());
    p.setStandardErrorFile(QProcess::nullDevice());
    p.start("python", params);
    p.waitForFinished(-1);

    if (p.error() == QProcess::UnknownError && p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0)
    {
        QMessageBox::information(this,"Build successful!", "Build was completed successfully!");
    }
    else
    {
        QMessageBox::critical(this, "Build failed!", "Build failed!\nPlease check your dou and CMakeLists.txt files for errors.");
    }

//TODO: split into separate thread
    //make the error box have one button saying "open log"
    //add support for the show log button.
}

void Dobmake::on_buildAndInstall_clicked()
{

}

void Dobmake::on_debugRadioButton_clicked(bool checked)
{
    m_debug = checked;
    m_release = !checked;
}

void Dobmake::on_releaseRadioButton_clicked(bool checked)
{
    m_debug = !checked;
    m_release = checked;
}

void Dobmake::on_debugCheckButton_clicked(bool checked)
{
    m_debug = checked;
}

void Dobmake::on_releaseCheckButton_clicked(bool checked)
{
    m_release = checked;
}
