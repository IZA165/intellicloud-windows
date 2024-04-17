//
//  Logger+Extensions.swift
//  FileProviderUIExt
//
//  Created by Claudio Cambra on 21/2/24.
//

import OSLog

extension Logger {
    private static var subsystem = Bundle.main.bundleIdentifier!

    static let actionViewController = Logger(subsystem: subsystem, category: "actionViewController")
    static let sharesDataSource = Logger(subsystem: subsystem, category: "sharesDataSource")
    static let shareViewController = Logger(subsystem: subsystem, category: "shareViewController")
}

