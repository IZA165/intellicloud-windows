/*
 * Copyright (C) by Olivier Goffart <ogoffart@woboq.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "accountmanager.h"
#include "sslerrordialog.h"
#include <theme.h>
#include <creds/credentialsfactory.h>
#include <creds/abstractcredentials.h>
#include <cookiejar.h>
#include <QSettings>
#include <QDir>
#include <QNetworkAccessManager>

namespace {
static const char urlC[] = "url";
static const char authTypeC[] = "authType";
static const char userC[] = "user";
static const char httpUserC[] = "http_user";
static const char caCertsKeyC[] = "CaCertificates";
}


namespace OCC {

AccountManager *AccountManager::instance()
{
    static AccountManager instance;
    return &instance;
}

bool AccountManager::restore()
{
    // try to open the correctly themed settings
    QScopedPointer<QSettings> settings(Account::settingsWithGroup(Theme::instance()->appName()));

    AccountPtr acc;
    bool migratedCreds = false;

    // if the settings file could not be opened, the childKeys list is empty
    if( settings->childKeys().isEmpty() ) {
        // Now try to open the original ownCloud settings to see if they exist.
        QString oCCfgFile = QDir::fromNativeSeparators( settings->fileName() );
        // replace the last two segments with ownCloud/owncloud.cfg
        oCCfgFile = oCCfgFile.left( oCCfgFile.lastIndexOf('/'));
        oCCfgFile = oCCfgFile.left( oCCfgFile.lastIndexOf('/'));
        oCCfgFile += QLatin1String("/ownCloud/owncloud.cfg");

        qDebug() << "Migrate: checking old config " << oCCfgFile;

        QFileInfo fi( oCCfgFile );
        if( fi.isReadable() ) {
            QSettings *oCSettings = new QSettings(oCCfgFile, QSettings::IniFormat);
            oCSettings->beginGroup(QLatin1String("ownCloud"));

            // Check the theme url to see if it is the same url that the oC config was for
            QString overrideUrl = Theme::instance()->overrideServerUrl();
            if( !overrideUrl.isEmpty() ) {
                if (overrideUrl.endsWith('/')) { overrideUrl.chop(1); }
                QString oCUrl = oCSettings->value(QLatin1String(urlC)).toString();
                if (oCUrl.endsWith('/')) { oCUrl.chop(1); }

                // in case the urls are equal reset the settings object to read from
                // the ownCloud settings object
                qDebug() << "Migrate oC config if " << oCUrl << " == " << overrideUrl << ":"
                << (oCUrl == overrideUrl ? "Yes" : "No");
                if( oCUrl == overrideUrl ) {
                    migratedCreds = true;
                    settings.reset( oCSettings );
                } else {
                    delete oCSettings;
                }
            }
        }
    }

    if (!settings->childKeys().isEmpty()) {
        acc = AccountPtr(new Account);
        acc->setSharedThis(acc);

        acc->setUrl(settings->value(QLatin1String(urlC)).toUrl());
        acc->setCredentials(CredentialsFactory::create(settings->value(QLatin1String(authTypeC)).toString()));

        // We want to only restore settings for that auth type and the user value
        acc->_settingsMap.insert(QLatin1String(userC), settings->value(userC));
        QString authTypePrefix = settings->value(authTypeC).toString() + "_";
        Q_FOREACH(QString key, settings->childKeys()) {
            if (!key.startsWith(authTypePrefix))
                continue;
            acc->_settingsMap.insert(key, settings->value(key));
        }

        // now the cert, it is in the general group
        settings->beginGroup(QLatin1String("General"));
        acc->setApprovedCerts(QSslCertificate::fromData(settings->value(caCertsKeyC).toByteArray()));
        acc->setMigrated(migratedCreds);
        acc->setSslErrorHandler(new SslDialogErrorHandler);
        addAccount(acc);
        return true;
    }
    return false;
}


void AccountManager::save()
{
    foreach (const auto &acc , _accounts) {
        save(acc->account());
    }
}

void AccountManager::save(const AccountPtr& acc)
{
    QScopedPointer<QSettings> settings(Account::settingsWithGroup(Theme::instance()->appName()));
    settings->setValue(QLatin1String(urlC), acc->_url.toString());
    if (acc->_credentials) {
        acc->_credentials->persist();
        Q_FOREACH(QString key, acc->_settingsMap.keys()) {
            settings->setValue(key, acc->_settingsMap.value(key));
        }
        settings->setValue(QLatin1String(authTypeC), acc->_credentials->authType());

        // HACK: Save http_user also as user
        if (acc->_settingsMap.contains(httpUserC))
            settings->setValue(userC, acc->_settingsMap.value(httpUserC));
    }
    settings->sync();

    // Save accepted certificates.
    settings->beginGroup(QLatin1String("General"));
    qDebug() << "Saving " << acc->approvedCerts().count() << " unknown certs.";
    QByteArray certs;
    Q_FOREACH( const QSslCertificate& cert, acc->approvedCerts() ) {
        certs += cert.toPem() + '\n';
    }
    if (!certs.isEmpty()) {
        settings->setValue( QLatin1String(caCertsKeyC), certs );
    }

    // Save cookies.
    if (acc->_am) {
        CookieJar* jar = qobject_cast<CookieJar*>(acc->_am->cookieJar());
        if (jar) {
            qDebug() << "Saving cookies.";
            jar->save();
        }
    }
}

void AccountManager::addAccount(const AccountPtr& newAccount)
{
    AccountStatePtr newAccountState(new AccountState(newAccount));
    _accounts << newAccountState;
    emit accountAdded(newAccountState.data());
}

void AccountManager::shutdown()
{
    auto accountsCopy = _accounts;
    _accounts.clear();
    foreach (const auto &acc, accountsCopy) {
        emit accountRemoved(acc.data());
    }
}


}