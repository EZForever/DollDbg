# DollDbg

*A framework for dynamic instrumentation of Win32 API based on inline hooks*

> [!NOTE]
> This repository is part of my *Year-end Things Clearance 2024* code dump, which implies the following things:
> 
> 1. The code in this repo is from one of my personal hobby projects (mainly) developed in 2024
> 2. Since it's a hobby project, expect PoC / alpha-quality code, and an *astonishing* lack of documentation
> 3. The project is archived by the time this repo goes public; there will (most likely) be no future updates
> 
> In short, if you wish to use the code here for anything good, think twice.

This is the remade/enhanced version of my [PEDoll](https://github.com/EZForever/PEDoll), developed back in 2022 as my college graduation project, and didn't get published ever since. Comparing with the original version, this version features a more streamlined CLI experience, WinDbg integration, Python API, and more.

No documentation is available, however, as that would be my graduation thesis, and I'm not sure if that could go public yet. This repo is pretty self-explanatory when it comes to how to build the tool; you need Python for the client (Controller in old version's terms), and Visual Studio with C++ and CMake capabilities for everything else. Use `DollDbg.Client/requirements.txt` and `Vendor/fetch_vendor_repos.cmd` for dependencies.

Thesis abstract for reference:

> Whether for testing, debugging, profiling, behavioral analysis, or other research purposes, instrumentation of existing applications is one of the most common and important approaches for researchers. When the source code of the application cannot be obtained, the instrumentation process usually needs to determine the instrumentation point through static analysis or dynamic analysis in advance, and then use techniques such as binary editing or code injection to complete. However, with the widespread application of computer software protection technology in applications, the difficulty and workload of static analysis and instrumentation of protected applications increases greatly. Dynamic analysis is relatively less affected by software protection techniques, but the high coupling of dynamic analysis to the operating system environment and the uncertainty brought about by the execution of application code make the dynamic instrumentation process highly specialized. Aiming at the characteristics of Win32 API, an important target of application instrumentation on Windows platform, combined with the understanding of the demand background and the research of previous work, this paper develops a framework for Win32 API dynamic instrumentation using inline hooks and non-intrusive debugging technology. The framework solves the problems of remote dynamic instrumentation, debugging symbol support and trace minimization, which can meet the relevant needs of researchers and has certain practical significance.

