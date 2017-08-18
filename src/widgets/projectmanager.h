#ifndef PROJECTMANAGER_H
#define PROJECTMANAGER_H

#include <QListWidgetItem>
#include <QFutureWatcher>
#include <QProgressDialog>

#include "../dialogs/progressdialog.h"

namespace Ui {
    class ProjectManager;
}

class aiScene;

struct ModelData {
    ModelData() = default;
    ModelData(QString p, const aiScene *ai) : path(p), data(ai) {}
    QString path;
    const aiScene *data;
};

class SettingsManager;

#include <QListWidget>
#include <QTreeWidgetItem>
#include <QWidget>
#include <QFileDialog>
#include <QMessageBox>

class MainWindow;

class ProjectManager : public QWidget
{
    Q_OBJECT

public:
    ProjectManager(QWidget *parent = nullptr);
    ~ProjectManager();

    void prepareStore(QString path);
    void walkFileSystem(QString folder, QString path);
    QVector<ModelData> fetchModel(const QString &path);
    bool copyDirectoryFiles(const QString &fromDir, const QString &toDir, bool coverFileIfExist);
    void update();

    QString loadProjectDelegate();

    void test();
    void resizeEvent(QResizeEvent*);

protected slots:
    void listWidgetCustomContextMenu(const QPoint&);
    void removeFromList();
    void deleteProject();
    void openProject();
    void openSampleProject(QListWidgetItem*);
    void renameItem(QListWidgetItem*);
    void openRecentProject(QListWidgetItem*);
    void newProject();

    void renameProject();
    void updateCurrentItem(QListWidgetItem*);

    void handleDone();
    void handleDoneFuture();

    void OnLstItemsCommitData(QWidget*);

signals:
    void fileToOpen(const QString& str);
    void fileToCreate(const QString& str, const QString& str2);

private:
    Ui::ProjectManager *ui;
    SettingsManager* settings;

    MainWindow *window;

//    Database *db;

    QListWidgetItem *currentItem;
//    QProgressDialog *dialog;
    QString pathToOpen;
    QFutureWatcher<QVector<ModelData>> *futureWatcher;

    QSharedPointer<ProgressDialog> progressDialog;
    bool isNewProject;
    bool isMainWindowActive;
};

struct AssetWidgetConcurrentWrapper {
    ProjectManager *instance;
    typedef QVector<ModelData> result_type;
    AssetWidgetConcurrentWrapper(ProjectManager *inst) : instance(inst) {}
        result_type operator()(const QString &data) {
        return instance->fetchModel(data);
    }
};

#endif // PROJECTMANAGER_H
