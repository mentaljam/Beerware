#include "beersmodel.h"
#include "beer.h"

#include <sailfishapp.h>
#include <QtQuick>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>


BeersModel::BeersModel(QObject *parent) :
    QAbstractListModel(parent)
{
    auto dbPath = QDir::cleanPath(SailfishApp::createView()->engine()->offlineStoragePath() +
                                  QLatin1String("/../../beers.sqlite"));
    auto db = QSqlDatabase::addDatabase(QLatin1String("QSQLITE"));
    db.setDatabaseName(dbPath);
    QFileInfo dbFileInfo(dbPath);
    auto canOpen = dbFileInfo.exists();
    // Open existing database
    if (canOpen)
    {
        db.open();
    }
    if (!canOpen)
    {
        qDebug() << "Searching for an old database";
        auto oldDbDir = QDir(SailfishApp::createView()->engine()->offlineStoragePath() +
                             QLatin1String("/Databases"));
        auto entries = oldDbDir.entryList(QDir::Files);
        if (!entries.isEmpty())
        {
            QString version;
            foreach (QString e, entries)
            {
                auto suffix = QFileInfo(e).suffix();
                auto filePath = oldDbDir.absoluteFilePath(e);
                if (suffix == QLatin1String("sqlite"))
                {
                    QFile oldDb(filePath);
                    if (oldDb.copy(dbPath))
                    {
                        canOpen = true;
                    }
                    else
                    {
                        qCritical() << "Could not move database file:" << oldDb.errorString();
                    }
                }
                else if (suffix == QLatin1String("ini"))
                {
                    version = QSettings(filePath, QSettings::IniFormat)
                                .value(QLatin1String("Version")).toString();
                }
            }
            if (canOpen)
            {
                db.open();
                this->initInfoTable(version);
            }
        }
    }
    if (!canOpen)
    {
        qDebug() << "Creating new database";
        dbFileInfo.absoluteDir().mkpath(QLatin1Literal("."));
        db.open();
        db.exec("CREATE TABLE IF NOT EXISTS beers ("
                "uID INTEGER PRIMARY KEY,"
                "name TEXT, category TEXT,"
                "rating INTEGER,"
                "UNIQUE (name, category, rating)"
                "ON CONFLICT REPLACE)");
        this->initInfoTable(QLatin1String("0.8.2"));
    }
    this->checkDbVersion();
    this->select();
}

BeersModel::~BeersModel()
{
    qDeleteAll(mData);
}

bool BeersModel::addBeer(const QString &name, const QString &category, const int &rating)
{
    QSqlQuery query;
    query.prepare(QLatin1String("INSERT OR IGNORE INTO beers VALUES (NULL, ?, ?, ?)"));
    query.bindValue(0, name);
    query.bindValue(1, category);
    query.bindValue(2, rating);
    if (!query.exec())
    {
        qWarning() << query.lastError();
        return false;
    }

    if (query.numRowsAffected())
    {
        auto ind = mData.size();
        this->beginInsertRows(QModelIndex(), ind, ind);
        if (!query.exec(QLatin1String("SELECT uID FROM beers ORDER BY uID DESC LIMIT 1")))
        {
            qWarning() << query.lastError();
            return false;
        }
        query.first();
        mData.append(new Beer(query.value(0).toInt(), name, category, rating));
        this->endInsertRows();
    }
    return true;
}

bool BeersModel::updateBeer(Beer *beer)
{
    QSqlQuery query;
    query.prepare(QLatin1String("REPLACE INTO beers VALUES (?, ?, ?, ?)"));
    query.bindValue(0, beer->mUId);
    query.bindValue(1, beer->mName);
    query.bindValue(2, beer->mCategory);
    query.bindValue(3, beer->mRating);
    if (!query.exec())
    {
        qWarning() << query.lastError();
        return false;
    }

    // This need when one changes beer so it's equal to another
    // in this case two beers merge and we need to update model
    if (!query.exec("SELECT Count(*) AS count FROM beers"))
    {
        qWarning() << query.lastError();
        return false;
    }
    if (query.first() && query.value(0) != mData.size())
    {
        this->beginResetModel();
        qDeleteAll(mData);
        mData.clear();
        this->select();
        this->endResetModel();
    }

    return true;
}

bool BeersModel::removeBeer(const int &index)
{
    Q_ASSERT(index >=0 && index < mData.size());

    QSqlQuery query;
    auto beer = mData.at(index);
    query.prepare(QLatin1String("DELETE FROM beers WHERE uID=(?)"));
    query.bindValue(0, beer->mUId);
    if(!query.exec())
    {
        qWarning() << query.lastError();
        return false;
    }
    this->beginRemoveRows(QModelIndex(), index, index);
    beer->deleteLater();
    mData.removeAt(index);
    this->endRemoveRows();

    return true;
}

Beer *BeersModel::getBeer(const int &row)
{
    Q_ASSERT(row >= 0 && row < mData.size());

    return mData.at(row);
}

QStringList BeersModel::categories() const
{
    QSqlQuery query;
    QStringList sl;
    if (!query.exec(QLatin1String("SELECT DISTINCT category FROM beers "
                                  "ORDER BY category COLLATE NOCASE")))
    {
        qWarning() << query.lastError();
        return sl;
    }
    while (query.next())
    {
        sl.append(query.value(0).toString());
    }
    return sl;
}

bool BeersModel::exportBeers() const
{
    if (mData.isEmpty())
    {
        return false;
    }
    QFile exportFile(QStringLiteral("%0/Beerware_%1.csv").arg(
                         QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
                         QDateTime::currentDateTime().toString(QLatin1String("yyyyMMddhhmmss"))));
    if (exportFile.open(QFile::WriteOnly))
    {
        QTextStream writer(&exportFile);
        writer.setCodec("UTF8");
        writer << "name,category,rating\n";
        foreach (Beer *beer, mData)
        {
            writer << beer->mName << ',' << beer->mCategory << ',' << beer->mRating << '\n';
        }
        exportFile.close();
        return true;
    }
    return false;
}

void BeersModel::select()
{
    QSqlQuery query;
    if (!query.exec(QLatin1String("SELECT * FROM beers")))
    {
        qWarning() << query.lastError();
        return;
    }

    while (query.next())
    {
        mData.append(new Beer(
                            query.value(0).toInt(),
                            query.value(1).toString(),
                            query.value(2).toString(),
                            (quint8)query.value(3).toChar().toLatin1()
                         ));
    }
}

void BeersModel::initInfoTable(const QString &version)
{
    QSqlQuery query;
    query.exec(QLatin1String("CREATE TABLE IF NOT EXISTS `info` ("
                             "`Key` TEXT NOT NULL UNIQUE,"
                             "`Value` TEXT,"
                             "PRIMARY KEY(Key)"
                             ") WITHOUT ROWID"));
    query.prepare(QLatin1String("INSERT INTO info VALUES ('Version', ?)"));
    query.bindValue(0, version);
    query.exec();
}

void BeersModel::checkDbVersion()
{
    QSqlQuery query;
    query.exec(QLatin1String("SELECT Version FROM info"));
    query.first();
    auto version = query.value(0).toString();
    if (version == "0.8")
    {
        query.exec(QLatin1String("CREATE TABLE IF NOT EXISTS beers2 ("
                                 "uID INTEGER PRIMARY KEY,"
                                 "name TEXT,"
                                 "category TEXT,"
                                 "rating INTEGER,"
                                 "UNIQUE (name, category, rating)"
                                 "ON CONFLICT REPLACE)"));
        query.exec(QLatin1String("INSERT INTO beers2 SELECT * FROM beers"));
        query.exec(QLatin1String("DROP TABLE beers"));
        query.exec(QLatin1String("ALTER TABLE beers2 RENAME TO beers"));
        query.exec(QLatin1String("REPLACE INTO info VALUES('Version', '0.8.2')"));
    }
}

int BeersModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : mData.size();
}

QHash<int, QByteArray> BeersModel::roleNames() const
{
    return { { BeerRole, "beer" } };
}

QVariant BeersModel::data(const QModelIndex &index, int role) const
{
    auto row = index.row();
    if (!index.isValid() || row >= mData.size())
    {
        return QVariant();
    }
    auto beer = mData[row];
    if (role == BeerRole)
    {
        return QVariant::fromValue(beer);
    }
    else
    {
        auto categoryPrefix = beer->mCategory.isEmpty() ? "0" : beer->mCategory;
        auto beerRating = QString::number(5 - beer->mRating);
        switch (role)
        {
        case CategoryNameRatingSortRole:
            return categoryPrefix + beer->mName + beerRating;
        case CategoryRatingNameSortRole:
            return categoryPrefix + beerRating + beer->mName;
        default:
            return QVariant();
        }
    }
}
