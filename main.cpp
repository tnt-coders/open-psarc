#include "PsarcFile.h"
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QFileInfo>

void listFiles(const QString& psarcPath)
{
    PsarcFile psarc(psarcPath);

    if (!psarc.open()) {
        qCritical() << "Failed to open:" << psarc.getLastError();
        return;
    }

    QStringList files = psarc.getFileList();
    qInfo() << "Archive contains" << files.size() << "files:";
    qInfo() << "";

    for (const QString& file : files) {
        qInfo().noquote() << file;
    }
}

void extractFile(const QString& psarcPath, const QString& fileName, const QString& outputPath)
{
    PsarcFile psarc(psarcPath);

    if (!psarc.open()) {
        qCritical() << "Failed to open:" << psarc.getLastError();
        return;
    }

    if (!psarc.fileExists(fileName)) {
        qCritical() << "File not found in archive:" << fileName;
        qInfo() << "Available files:";
        for (const QString& file : psarc.getFileList()) {
            if (file.contains(fileName, Qt::CaseInsensitive)) {
                qInfo() << "  " << file;
            }
        }
        return;
    }

    if (psarc.extractFileTo(fileName, outputPath)) {
        QFileInfo info(outputPath);
        qInfo() << "Extracted to:" << outputPath;
        qInfo() << "File size:" << info.size() << "bytes";
    } else {
        qCritical() << "Extraction failed:" << psarc.getLastError();
    }
}

void extractAll(const QString& psarcPath, const QString& outputDir)
{
    PsarcFile psarc(psarcPath);

    if (!psarc.open()) {
        qCritical() << "Failed to open:" << psarc.getLastError();
        return;
    }

    qInfo() << "Extracting" << psarc.getFileList().size() << "files to" << outputDir;

    if (psarc.extractAll(outputDir)) {
        qInfo() << "All files extracted successfully";
    } else {
        qCritical() << "Extraction failed:" << psarc.getLastError();
    }
}

void searchFiles(const QString& psarcPath, const QString& pattern)
{
    PsarcFile psarc(psarcPath);

    if (!psarc.open()) {
        qCritical() << "Failed to open:" << psarc.getLastError();
        return;
    }

    QStringList matches;
    for (const QString& file : psarc.getFileList()) {
        if (file.contains(pattern, Qt::CaseInsensitive)) {
            matches.append(file);
        }
    }

    qInfo() << "Found" << matches.size() << "matching files:";
    for (const QString& file : matches) {
        qInfo().noquote() << file;
    }
}

void showInfo(const QString& psarcPath)
{
    PsarcFile psarc(psarcPath);

    if (!psarc.open()) {
        qCritical() << "Failed to open:" << psarc.getLastError();
        return;
    }

    QStringList files = psarc.getFileList();

    qint64 totalSize = 0;
    QMap<QString, int> extensionCount;

    for (const QString& file : files) {
        QFileInfo info(file);
        QString ext = info.suffix().toLower();
        if (ext.isEmpty()) ext = "(no extension)";

        extensionCount[ext]++;
    }

    qInfo() << "Archive Information:";
    qInfo() << "  File path:" << psarcPath;
    qInfo() << "  Total files:" << files.size();
    qInfo() << "";
    qInfo() << "File types:";

    for (auto it = extensionCount.begin(); it != extensionCount.end(); ++it) {
        qInfo().noquote() << QString("  .%1: %2 files").arg(it.key()).arg(it.value());
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("psarc-test");
    QCoreApplication::setApplicationVersion("1.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("PSARC Archive Tool - Test Program");
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addPositionalArgument("command", "Command to execute: list, extract, extract-all, search, info");
    parser.addPositionalArgument("psarcfile", "Path to PSARC archive");

    QCommandLineOption fileOption(QStringList() << "f" << "file",
                                  "File to extract (for extract command)",
                                  "filename");
    parser.addOption(fileOption);

    QCommandLineOption outputOption(QStringList() << "o" << "output",
                                    "Output path/directory",
                                    "path");
    parser.addOption(outputOption);

    QCommandLineOption patternOption(QStringList() << "p" << "pattern",
                                     "Search pattern (for search command)",
                                     "pattern");
    parser.addOption(patternOption);

    parser.process(app);

    const QStringList args = parser.positionalArguments();

    if (args.size() < 2) {
        qCritical() << "Error: Missing required arguments";
        parser.showHelp(1);
        return 1;
    }

    QString command = args[0].toLower();
    QString psarcPath = args[1];

    if (!QFileInfo::exists(psarcPath)) {
        qCritical() << "Error: File does not exist:" << psarcPath;
        return 1;
    }

    if (command == "list") {
        listFiles(psarcPath);
    }
    else if (command == "extract") {
        QString fileName = parser.value(fileOption);
        QString output = parser.value(outputOption);

        if (fileName.isEmpty() || output.isEmpty()) {
            qCritical() << "Error: extract command requires --file and --output";
            return 1;
        }

        extractFile(psarcPath, fileName, output);
    }
    else if (command == "extract-all") {
        QString output = parser.value(outputOption);

        if (output.isEmpty()) {
            qCritical() << "Error: extract-all command requires --output";
            return 1;
        }

        extractAll(psarcPath, output);
    }
    else if (command == "search") {
        QString pattern = parser.value(patternOption);

        if (pattern.isEmpty()) {
            qCritical() << "Error: search command requires --pattern";
            return 1;
        }

        searchFiles(psarcPath, pattern);
    }
    else if (command == "info") {
        showInfo(psarcPath);
    }
    else {
        qCritical() << "Error: Unknown command:" << command;
        qCritical() << "Valid commands: list, extract, extract-all, search, info";
        return 1;
    }

    return 0;
}

/*
Usage Examples:

# List all files in archive
./psarc-test list songs.psarc

# Show archive info
./psarc-test info songs.psarc

# Search for files
./psarc-test search songs.psarc --pattern .json

# Extract specific file
./psarc-test extract songs.psarc --file "manifests/song.json" --output output.json

# Extract all files
./psarc-test extract-all songs.psarc --output ./extracted/

*/
