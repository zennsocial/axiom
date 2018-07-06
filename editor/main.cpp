#include <QtCore/QFile>

#include "AxiomApplication.h"
#include "StandaloneAudio.h"
#include "compiler/interface/Frontend.h"
#include "compiler/interface/Jit.h"
#include "compiler/interface/Runtime.h"
#include "model/objects/RootSurface.h"
#include "widgets/surface/NodeSurfacePanel.h"
#include "widgets/windows/MainWindow.h"

int main(int argc, char *argv[]) {
    MaximFrontend::maxim_initialize();
    MaximCompiler::Jit jit;
    MaximCompiler::Runtime runtime(true, false, &jit);

    auto project = std::make_unique<AxiomModel::Project>();
    project->mainRoot().attachRuntime(&runtime);

    AxiomGui::MainWindow window(std::move(project));
    // AxiomStandalone::startupAudio();
    window.show();
    auto result = AxiomApplication::main.exec();
    // AxiomStandalone::shutdownAudio();
    return result;
}
