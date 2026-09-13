$content = Get-Content -Path d:\TearDown\src\player\Player.cpp -Raw
$content = $content -replace 'void processInput\(GLFWwindow\*\swindow\)\s*\{', "void applyTerrainDestruction(const glm::vec3&, int);
void processInput(GLFWwindow* window) {"
Set-Content -Path d:\TearDown\src\player\Player.cpp -Value $content
