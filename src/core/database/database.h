#ifndef DATABASE_H
#define DATABASE_H

#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>
#include <QCryptographicHash>

#include "../project.h"

class Database
{
public:
    Database();
    ~Database();

    bool executeAndCheckQuery(QSqlQuery&, const QString&);
    void initializeDatabase(QString name);
    void closeDb();

    bool checkIfTableExists(const QString &tableName);

	void createGlobalDependencies();
	void insertGlobalDependency(const int &type, const QString &depender, const QString &dependee, const QString &project_id = QString());

	QString getDependencyByType(const int &type, const QString &depender);

    void createGlobalDb();
    void createGlobalDbThumbs();
    void createGlobalDbCollections();
	void createGlobalDbAssets();
	void createGlobalDbMaterials();
	void createGlobalDbAuthor();
	void createGlobalDbProjectAssets();
	void updateAuthorInfo(const QString &author_name);
	bool isAuthorInfoPresent();
	QString getAuthorName();
    void deleteProject();
	bool canDeleteAsset(const QString &guid);
	bool softDeleteAsset(const QString &guid);
    bool deleteAsset(const QString &guid);
    void renameProject(const QString&);
	void updateAssetThumbnail(const QString guid, const QByteArray &thumbnail);
	QString insertAssetGlobal(const QString&, int type, const QByteArray &thumbnail, const QByteArray &properties, const QByteArray &tags);
	void insertProjectAssetGlobal(const QString&, int type, const QByteArray &thumbnail, const QByteArray &properties, const QByteArray &tags, const QString &guid);
	QString insertMaterialGlobal(const QString &materialName, const QString &asset_guid, const QByteArray &material, bool used = false);
    void insertSceneGlobal(const QString &world_guid, const QByteArray &sceneBlob, const QByteArray &thumb);
    void insertCollectionGlobal(const QString &collectionName);
	void updateAssetUsed(const QString &guid, bool used);
    bool switchAssetCollection(const int, const QString&);
    void insertThumbnailGlobal(const QString &world_guid,
                               const QString &name,
                               const QByteArray &thumbnail,
							   const QString &thumbnail_guid);
	QByteArray getMaterialGlobal(const QString &guid) const;
    bool hasCachedThumbnail(const QString& name);
	QVector<AssetData> fetchThumbnails();
    QVector<CollectionData> fetchCollections();
    QVector<ProjectTileData> fetchProjects();
	QVector<AssetTileData> fetchAssets();
	QVector<AssetTileData> fetchAssetsByCollection(int collection_id);
    QByteArray getSceneBlobGlobal() const;
    QByteArray fetchCachedThumbnail(const QString& name) const;
	void updateAssetMetadata(const QString &guid, const QString &name, const QByteArray &tags);
    void updateSceneGlobal(const QByteArray &sceneBlob, const QByteArray &thumbnail);
    void createExportScene(const QString& outTempFilePath);
    bool importProject(const QString& inFilePath);

    QSqlDatabase getDb() { return db; }

private:
    QSqlDatabase db;
};

#endif // DATABASE_H
